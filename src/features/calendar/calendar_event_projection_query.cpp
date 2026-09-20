#include "calendar_event_projection_query.h"

#include "data/database/database_schema_manager.h"
#include "data/repositories/calendar_event_repository.h"

#include <QByteArray>
#include <QSqlDatabase>
#include <QSqlError>
#include <QUuid>

#include <exception>
#include <optional>
#include <string>
#include <utility>

namespace
{
using Projection =
    CalendarEventProjectionQuery::Projection;
using ProjectionResult =
    CalendarEventProjectionQuery::ProjectionResult;
using NextEventDateResult =
    CalendarEventProjectionQuery::NextEventDateResult;
using Summary =
    ClassMngr::Next::Application::CalendarEventSummary;
using ProjectionInput =
    ClassMngr::Next::Application::CalendarEventProjectionInput;
using ErrorCode =
    ClassMngr::Next::Domain::ErrorCode;

std::string messageText(
    const QString& message,
    const char* fallback
    )
{
    const QByteArray bytes = message.toUtf8();
    return bytes.isEmpty()
        ? std::string(fallback)
        : bytes.toStdString();
}

template <typename Value>
ClassMngr::Next::Domain::Result<Value> failure(
    const ErrorCode code,
    const QString& message,
    const char* fallback
    )
{
    return ClassMngr::Next::Domain::Result<Value>::failure({
        .code = code,
        .message = messageText(message, fallback),
        .recoverable = false
    });
}

template <typename Value>
ClassMngr::Next::Domain::Result<Value> technicalFailure(
    const QString& message
    )
{
    return failure<Value>(
        ErrorCode::Technical,
        message,
        "Calendar event database query failed."
        );
}

std::optional<std::string> boundedUtf8(
    const QString& value,
    const std::size_t maximumBytes
    )
{
    if (value.trimmed().isEmpty())
    {
        return std::nullopt;
    }

    const QByteArray bytes = value.toUtf8();
    if (static_cast<std::size_t>(bytes.size()) > maximumBytes)
    {
        return std::nullopt;
    }

    return bytes.toStdString();
}

std::optional<std::string> optionalBoundedUtf8(
    const QString& value,
    const std::size_t maximumBytes
    )
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty())
    {
        return std::string{};
    }

    return boundedUtf8(trimmed, maximumBytes);
}

ProjectionResult projectionFromEvents(
    const QList<CalendarEvent>& events
    )
{
    ProjectionInput input;
    input.events.reserve(
        static_cast<std::size_t>(events.size())
        );

    for (const CalendarEvent& source : events)
    {
        // The legacy cache ignores records that cannot become a canonical
        // indexed event. Keep that filtering at the query boundary so a
        // malformed row does not prevent the rest of a valid range from
        // loading.
        if (
            source.id <= 0
            || !source.startDate.isValid()
            || !source.endDate.isValid()
            || source.endDate < source.startDate
            )
        {
            continue;
        }

        const auto eventId =
            ClassMngr::Next::Domain::CalendarEventId::fromString(
                std::to_string(source.id)
                );
        const auto title = boundedUtf8(
            source.title,
            ClassMngr::Next::Application::kCalendarEventSummaryMaxTitleLength
            );
        const auto startDate = boundedUtf8(
            source.startDate.toString(Qt::ISODate),
            ClassMngr::Next::Application::kCalendarEventSummaryMaxDateLength
            );
        const auto endDate = boundedUtf8(
            source.endDate.toString(Qt::ISODate),
            ClassMngr::Next::Application::kCalendarEventSummaryMaxDateLength
            );
        const auto eventType = boundedUtf8(
            source.eventType,
            ClassMngr::Next::Application::kCalendarEventSummaryMaxEventTypeLength
            );
        const auto timeStatus = boundedUtf8(
            source.timeStatus,
            ClassMngr::Next::Application::kCalendarEventSummaryMaxTimeStatusLength
            );

        const QString normalizedRepeatSeriesId =
            source.repeatSeriesId.trimmed();
        std::optional<std::string> repeatSeriesId;
        if (!normalizedRepeatSeriesId.isEmpty())
        {
            repeatSeriesId = boundedUtf8(
                normalizedRepeatSeriesId,
                ClassMngr::Next::Application::kCalendarEventSummaryMaxRepeatSeriesIdLength
                );
        }

        if (
            !eventId
            || !title
            || !startDate
            || !endDate
            || !eventType
            || !timeStatus
            || (
                !normalizedRepeatSeriesId.isEmpty()
                && !repeatSeriesId
                )
            )
        {
            return failure<Projection>(
                ErrorCode::InvalidInput,
                QStringLiteral(
                    "A calendar event contains invalid or unbounded projection metadata."
                    ),
                "A calendar event contains invalid or unbounded projection metadata."
                );
        }

        std::optional<std::string> startTime;
        std::optional<std::string> endTime;
        if (!source.allDay)
        {
            const bool hasStartTime = source.startTime.isValid();
            const bool hasEndTime = source.endTime.isValid();
            if (hasStartTime != hasEndTime)
            {
                return failure<Projection>(
                    ErrorCode::InvalidInput,
                    QStringLiteral(
                        "A timed calendar event has a partial time range."
                        ),
                    "A timed calendar event has a partial time range."
                    );
            }

            if (hasStartTime)
            {
                startTime = boundedUtf8(
                    source.startTime.toString(QStringLiteral("HH:mm")),
                    ClassMngr::Next::Application::kCalendarEventSummaryMaxTimeLength
                    );
                endTime = boundedUtf8(
                    source.endTime.toString(QStringLiteral("HH:mm")),
                    ClassMngr::Next::Application::kCalendarEventSummaryMaxTimeLength
                    );
                if (!startTime || !endTime)
                {
                    return failure<Projection>(
                        ErrorCode::InvalidInput,
                        QStringLiteral(
                            "A calendar event contains invalid or unbounded time metadata."
                            ),
                        "A calendar event contains invalid or unbounded time metadata."
                        );
                }
            }
        }

        input.events.push_back({
            *eventId,
            std::nullopt,
            std::nullopt,
            *title,
            *startDate,
            *endDate,
            std::move(startTime),
            std::move(endTime),
            std::string{},
            std::string{},
            static_cast<std::int32_t>(input.events.size()),
            source.allDay,
            *eventType,
            *timeStatus,
            std::move(repeatSeriesId)
        });
    }

    const auto projection = Projection::create(std::move(input));
    if (!projection)
    {
        return failure<Projection>(
            projection.error().code,
            QString::fromUtf8(projection.error().message.c_str()),
            "Calendar event projection validation failed."
            );
    }

    return projection;
}

template <typename Value, typename Query>
ClassMngr::Next::Domain::Result<Value> runWithDatabase(
    QString databasePath,
    Query query
    )
{
    const QString connectionName =
        QStringLiteral("calendar-event-projection-query-%1").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            );

    std::optional<ClassMngr::Next::Domain::Result<Value>> result;
    try
    {
        result = [&]()
        {
            QSqlDatabase database =
                QSqlDatabase::addDatabase(
                    QStringLiteral("QSQLITE"),
                    connectionName
                    );
            database.setDatabaseName(databasePath);

            if (!database.open())
            {
                return technicalFailure<Value>(
                    database.lastError().text()
                    );
            }

            const Status foreignKeyStatus =
                DatabaseSchemaManager::enableForeignKeyEnforcement(
                    database
                    );
            if (!foreignKeyStatus)
            {
                database.close();
                return technicalFailure<Value>(
                    foreignKeyStatus.error()
                    );
            }

            auto loaded = query(database);
            database.close();
            return loaded;
        }();
    }
    catch (const std::exception& exception)
    {
        result = technicalFailure<Value>(
            QString::fromUtf8(exception.what())
            );
    }
    catch (...)
    {
        result = technicalFailure<Value>(QString());
    }

    QSqlDatabase::removeDatabase(connectionName);
    return std::move(*result);
}
}

CalendarEventProjectionQuery::ProjectionResult
CalendarEventProjectionQuery::loadRange(
    QString databasePath,
    QDate startDate,
    QDate endDate
    )
{
    if (
        databasePath.trimmed().isEmpty()
        || !startDate.isValid()
        || !endDate.isValid()
        || endDate < startDate
        )
    {
        return failure<Projection>(
            ErrorCode::InvalidInput,
            QStringLiteral(
                "Calendar event range must contain a database path and valid ordered dates."
                ),
            "Calendar event range must contain a database path and valid ordered dates."
            );
    }

    return runWithDatabase<Projection>(
        std::move(databasePath),
        [startDate, endDate](QSqlDatabase& database)
        {
            CalendarEventRepository repository(database);
            const ::Result<QList<CalendarEvent>> events =
                repository.loadCalendarEventsInRange(
                    startDate,
                    endDate
                    );
            if (!events)
            {
                return technicalFailure<Projection>(events.error());
            }

            return projectionFromEvents(*events);
        }
        );
}

CalendarEventProjectionQuery::NextEventDateResult
CalendarEventProjectionQuery::findNextEventDate(
    QString databasePath,
    QDate afterDate
    )
{
    if (databasePath.trimmed().isEmpty() || !afterDate.isValid())
    {
        return failure<QDate>(
            ErrorCode::InvalidInput,
            QStringLiteral(
                "Finding the next calendar event requires a database path and valid date."
                ),
            "Finding the next calendar event requires a database path and valid date."
            );
    }

    return runWithDatabase<QDate>(
        std::move(databasePath),
        [afterDate](QSqlDatabase& database)
        {
            CalendarEventRepository repository(database);
            const ::Result<QDate> nextEventDate =
                repository.findNextCalendarEventStartDate(afterDate);
            if (!nextEventDate)
            {
                return technicalFailure<QDate>(nextEventDate.error());
            }

            return ClassMngr::Next::Domain::Result<QDate>::success(
                *nextEventDate
                );
        }
        );
}
