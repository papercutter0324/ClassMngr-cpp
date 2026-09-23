#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/calendar_event_projection.h"

#include <QByteArray>
#include <QDate>
#include <QList>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter that copies the legacy calendar service result into a
// bounded, application-owned event projection. ApplicationServices remains
// caller-owned; no CalendarService, CalendarEvent, or other legacy pointer
// crosses this boundary.
class ApplicationServicesCalendarEventPort final
{
public:
    explicit ApplicationServicesCalendarEventPort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesCalendarEventPort(
        const ApplicationServicesCalendarEventPort&
        ) = delete;
    ApplicationServicesCalendarEventPort& operator=(
        const ApplicationServicesCalendarEventPort&
        ) = delete;
    ApplicationServicesCalendarEventPort(
        ApplicationServicesCalendarEventPort&&
        ) = delete;
    ApplicationServicesCalendarEventPort& operator=(
        ApplicationServicesCalendarEventPort&&
        ) = delete;

    [[nodiscard]] bool isAvailable() const noexcept
    {
        try
        {
            const CalendarService* service = m_services.calendarService();
            return service && service->isAvailable();
        }
        catch (...)
        {
            return false;
        }
    }

    [[nodiscard]] Domain::Result<Application::CalendarEventSummary>
    projectionById(
        const int eventId
        ) const
    {
        if (eventId <= 0)
        {
            return failureSummary(
                Domain::ErrorCode::InvalidInput,
                "Calendar event identifier must be positive."
                );
        }

        try
        {
            const CalendarService* service = m_services.calendarService();
            if (!service || !service->isAvailable())
            {
                return failureSummary(
                    Domain::ErrorCode::NotFound,
                    "The calendar service is unavailable."
                    );
            }

            const ::Result<CalendarEvent> loaded = service->event(eventId);
            if (!loaded)
            {
                const QString legacyError = loaded.error();
                if (!service->isAvailable())
                {
                    return failureSummary(
                        Domain::ErrorCode::NotFound,
                        "The calendar service is unavailable."
                        );
                }

                const QString normalizedError = legacyError.toLower();
                if (normalizedError.contains(QStringLiteral("no matching record"))
                    || normalizedError.contains(QStringLiteral("not found"))
                    || normalizedError.contains(QStringLiteral("does not exist")))
                {
                    return failureSummary(
                        Domain::ErrorCode::NotFound,
                        "The calendar event was not found."
                        );
                }

                const QByteArray errorBytes = legacyError.toUtf8();
                const std::string message = errorBytes.isEmpty()
                    ? "Calendar event could not be loaded."
                    : "Calendar event could not be loaded: "
                        + errorBytes.toStdString();
                return failureSummary(
                    Domain::ErrorCode::Technical,
                    message
                    );
            }

            if (loaded->id != eventId)
            {
                return failureSummary(
                    Domain::ErrorCode::InvalidInput,
                    "The calendar event identifier does not match the requested identifier."
                    );
            }

            return projectEvent(*loaded, 0);
        }
        catch (const std::exception&)
        {
            return failureSummary(
                Domain::ErrorCode::Technical,
                "Calendar event could not be projected."
                );
        }
        catch (...)
        {
            return failureSummary(
                Domain::ErrorCode::Technical,
                "Calendar event could not be projected."
                );
        }
    }

    [[nodiscard]] Domain::Result<Application::CalendarEventProjection>
    projection(
        const QDate& startDate,
        const QDate& endDate
        ) const
    {
        if (!startDate.isValid()
            || !endDate.isValid()
            || endDate < startDate)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Calendar event range must contain valid ordered dates."
                );
        }

        try
        {
            const CalendarService* service = m_services.calendarService();
            if (!service || !service->isAvailable())
            {
                return failure(
                    Domain::ErrorCode::NotFound,
                    "The calendar service is unavailable."
                    );
            }

            const ::Result<QList<CalendarEvent>> loaded =
                service->eventsInRange(startDate, endDate);
            if (!loaded)
            {
                const QString legacyError = loaded.error();
                if (!service->isAvailable())
                {
                    return failure(
                        Domain::ErrorCode::NotFound,
                        "The calendar service is unavailable."
                        );
                }

                const QByteArray errorBytes = legacyError.toUtf8();
                const std::string message = errorBytes.isEmpty()
                    ? "Calendar events could not be loaded."
                    : "Calendar events could not be loaded: "
                        + errorBytes.toStdString();
                return failure(
                    Domain::ErrorCode::Technical,
                    message
                    );
            }

            const QList<CalendarEvent>& events = loaded.value();
            if (events.size()
                > static_cast<qsizetype>(
                    Application::kCalendarEventProjectionMaxEvents
                    ))
            {
                return failure(
                    Domain::ErrorCode::InvalidInput,
                    "Calendar event range exceeds its bounded projection capacity."
                    );
            }

            Application::CalendarEventProjectionInput input;
            input.events.reserve(
                static_cast<std::size_t>(events.size())
                );

            for (qsizetype index = 0; index < events.size(); ++index)
            {
                const CalendarEvent& source = events.at(index);
                const auto event = projectEvent(
                    source,
                    static_cast<std::int32_t>(index)
                    );
                if (!event)
                {
                    return failure(
                        event.error().code,
                        event.error().message
                        );
                }

                input.events.push_back(event.value());
            }

            return Application::CalendarEventProjection::create(
                std::move(input)
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Calendar events could not be projected."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Calendar events could not be projected."
                );
        }
    }

private:
    [[nodiscard]] static Domain::Result<Application::CalendarEventSummary>
    projectEvent(
        const CalendarEvent& source,
        const std::int32_t order
        )
    {
        if (source.id <= 0)
        {
            return failureSummary(
                Domain::ErrorCode::InvalidInput,
                "A calendar event has an invalid legacy identifier."
                );
        }

        const auto eventId = Domain::CalendarEventId::fromString(
            std::to_string(source.id)
            );
        const auto title = boundedUtf8(
            source.title,
            Application::kCalendarEventSummaryMaxTitleLength
            );
        const auto startDateText = boundedUtf8(
            source.startDate.toString(Qt::ISODate),
            Application::kCalendarEventSummaryMaxDateLength
            );
        const auto endDateText = boundedUtf8(
            source.endDate.toString(Qt::ISODate),
            Application::kCalendarEventSummaryMaxDateLength
            );
        const auto eventType = boundedUtf8(
            source.eventType,
            Application::kCalendarEventSummaryMaxEventTypeLength
            );
        const auto timeStatus = boundedUtf8(
            source.timeStatus,
            Application::kCalendarEventSummaryMaxTimeStatusLength
            );
        const QString normalizedRepeatSeriesId =
            source.repeatSeriesId.trimmed();
        std::optional<std::string> repeatSeriesId;
        if (!normalizedRepeatSeriesId.isEmpty())
        {
            repeatSeriesId = boundedUtf8(
                normalizedRepeatSeriesId,
                Application::kCalendarEventSummaryMaxRepeatSeriesIdLength
                );
            if (!repeatSeriesId)
            {
                return failureSummary(
                    Domain::ErrorCode::InvalidInput,
                    "A calendar event has an invalid or unbounded repeat-series identifier."
                    );
            }
        }

        if (!eventId
            || !title
            || !startDateText
            || !endDateText
            || !eventType
            || !timeStatus)
        {
            return failureSummary(
                Domain::ErrorCode::InvalidInput,
                "A calendar event contains invalid or unbounded metadata."
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
                return failureSummary(
                    Domain::ErrorCode::InvalidInput,
                    "A timed calendar event has a partial time range."
                    );
            }

            if (hasStartTime)
            {
                startTime = source.startTime.toString(
                    QStringLiteral("HH:mm")
                    ).toStdString();
                endTime = source.endTime.toString(
                    QStringLiteral("HH:mm")
                    ).toStdString();
            }
        }

        Application::CalendarEventSummary summary{
            *eventId,
            std::nullopt,
            std::nullopt,
            *title,
            *startDateText,
            *endDateText,
            std::move(startTime),
            std::move(endTime),
            std::string{},
            std::string{},
            order,
            source.allDay,
            *eventType,
            *timeStatus,
            std::move(repeatSeriesId)
        };

        const auto validation = Application::CalendarEventProjection::validate(
            summary
            );
        if (!validation)
        {
            return failureSummary(
                validation.error().code,
                validation.error().message
                );
        }

        return Domain::Result<Application::CalendarEventSummary>::success(
            std::move(summary)
            );
    }

    [[nodiscard]] static std::optional<std::string> boundedUtf8(
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

    [[nodiscard]] static Domain::Result<Application::CalendarEventProjection>
    failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return Domain::Result<Application::CalendarEventProjection>::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    [[nodiscard]] static Domain::Result<Application::CalendarEventSummary>
    failureSummary(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return Domain::Result<Application::CalendarEventSummary>::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
