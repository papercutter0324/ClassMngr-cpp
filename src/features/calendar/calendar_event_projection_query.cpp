#include "calendar_event_projection_query.h"

#include "data/database/database_schema_manager.h"
#include "data/repositories/calendar_event_repository.h"
#include "next/platform/calendar_event_time_status_adapter.h"

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
using SummaryConversionResult =
    ClassMngr::Next::Domain::Result<std::optional<Summary>>;
using ErrorCode =
    ClassMngr::Next::Domain::ErrorCode;

QString fromUtf8(
    const std::string& value
    )
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

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

SummaryConversionResult summaryFromEvent(
    const CalendarEvent& source,
    const ClassMngr::Next::Application::CalendarEventOrder order
    )
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
        return SummaryConversionResult::success(std::nullopt);
    }

    const auto eventId =
        ClassMngr::Next::Domain::CalendarEventId::fromString(
            std::to_string(source.id)
            );
    const auto title = boundedUtf8(
        source.title,
        ClassMngr::Next::Application::
            kCalendarEventSummaryMaxTitleUtf8Bytes
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
    const auto timeStatus =
        ClassMngr::Next::Platform::calendarEventTimeStatusFromLegacyText(
            source.timeStatus
            );

    const QString normalizedRepeatSeriesId =
        source.repeatSeriesId.trimmed();
    std::optional<ClassMngr::Next::Domain::CalendarEventSeriesId>
        repeatSeriesId;
    if (!normalizedRepeatSeriesId.isEmpty())
    {
        const auto boundedRepeatSeriesId = boundedUtf8(
            normalizedRepeatSeriesId,
            ClassMngr::Next::Application::kCalendarEventSummaryMaxRepeatSeriesIdLength
            );
        if (boundedRepeatSeriesId)
        {
            repeatSeriesId =
                ClassMngr::Next::Domain::CalendarEventSeriesId::fromString(
                    *boundedRepeatSeriesId
                    );
        }
    }

    if (
        !eventId
        || !title
        || !startDate
        || !endDate
        || !eventType
        || (
            !normalizedRepeatSeriesId.isEmpty()
            && !repeatSeriesId
            )
        )
    {
        return failure<std::optional<Summary>>(
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
            return failure<std::optional<Summary>>(
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
                return failure<std::optional<Summary>>(
                    ErrorCode::InvalidInput,
                    QStringLiteral(
                        "A calendar event contains invalid or unbounded time metadata."
                        ),
                    "A calendar event contains invalid or unbounded time metadata."
                    );
            }
        }
    }

    return SummaryConversionResult::success(Summary{
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
        order,
        source.allDay,
        ClassMngr::Next::Application::CalendarEventTypeName(*eventType),
        timeStatus,
        std::move(repeatSeriesId)
    });
}

ProjectionResult projectionFromRange(
    CalendarEventRepository& repository,
    const QDate& startDate,
    const QDate& endDate
    )
{
    constexpr std::size_t retainedEventLimit =
        ClassMngr::Next::Application::kCalendarEventProjectionMaxEvents + 1;

    ProjectionInput input;

    std::optional<ClassMngr::Next::Domain::OperationError> conversionError;
    const Status visited = repository.visitCalendarEventsInRange(
        startDate,
        endDate,
        [&input, &conversionError, retainedEventLimit](
            const CalendarEvent& source
            )
        {
            if (conversionError)
            {
                return CalendarEventRangeVisitDecision::Continue;
            }

            auto converted = summaryFromEvent(
                source,
                ClassMngr::Next::Application::CalendarEventOrder(
                    static_cast<std::int32_t>(input.events.size())
                    )
                );
            if (!converted)
            {
                conversionError = converted.error();
                return CalendarEventRangeVisitDecision::Continue;
            }

            if (
                converted.value()
                && input.events.size() < retainedEventLimit
                )
            {
                if (input.events.size() == input.events.capacity())
                {
                    const std::size_t requiredCapacity =
                        input.events.size() + 1;
                    std::size_t nextCapacity = input.events.capacity();
                    if (nextCapacity == 0)
                    {
                        nextCapacity = 1;
                    }
                    else if (nextCapacity <= retainedEventLimit / 2)
                    {
                        nextCapacity *= 2;
                    }
                    else
                    {
                        nextCapacity = retainedEventLimit;
                    }

                    if (nextCapacity < requiredCapacity)
                    {
                        nextCapacity = requiredCapacity;
                    }

                    // Keep allocation on demand and cap every reserve request
                    // at the projection limit plus its overflow sentinel.
                    input.events.reserve(nextCapacity);
                }

                input.events.push_back(std::move(*converted.value()));
            }

            return CalendarEventRangeVisitDecision::Continue;
        }
        );

    if (!visited)
    {
        return technicalFailure<Projection>(visited.error());
    }

    if (conversionError)
    {
        return ProjectionResult::failure(std::move(*conversionError));
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
    const ClassMngr::Next::Application::CalendarEventRangeRequest& request
    )
{
    return CalendarEventProjectionQuery::loadRange(
        fromUtf8(request.databasePath.value()),
        QDate::fromString(fromUtf8(request.startDate.value()), Qt::ISODate),
        QDate::fromString(fromUtf8(request.endDate.value()), Qt::ISODate)
        );
}

CalendarEventProjectionQuery::QueryDateResult
CalendarEventProjectionQuery::findNextEventDate(
    const ClassMngr::Next::Application::CalendarEventNextEventRequest& request
    )
{
    const NextEventDateResult result =
        CalendarEventProjectionQuery::findNextEventDate(
            fromUtf8(request.databasePath.value()),
            QDate::fromString(fromUtf8(request.afterDate.value()), Qt::ISODate)
            );
    if (!result)
    {
        return QueryDateResult::failure(result.error());
    }

    if (!result.value().isValid())
    {
        // A successful lookup with no matching event retains the legacy
        // invalid-date sentinel while staying a typed value-only result.
        return QueryDateResult::success(QueryDate{});
    }

    return QueryDateResult::success(
        QueryDate(
            result.value().toString(Qt::ISODate).toUtf8().toStdString()
            )
        );
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
            return projectionFromRange(repository, startDate, endDate);
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

std::unique_ptr<ClassMngr::Next::Application::CalendarEventQueryPort>
CalendarEventProjectionQueryFactory::create() const
{
    return std::make_unique<CalendarEventProjectionQuery>();
}
