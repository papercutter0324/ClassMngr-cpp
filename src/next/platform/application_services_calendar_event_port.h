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
                if (source.id <= 0)
                {
                    return failure(
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
                if (!eventId || !title || !startDateText || !endDateText)
                {
                    return failure(
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
                        return failure(
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

                input.events.push_back({
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
                    static_cast<std::int32_t>(index),
                    source.allDay
                });
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

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
