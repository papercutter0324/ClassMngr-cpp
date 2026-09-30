#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/calendar_event_repository.h"
#include "next/application/calendar_event_delete_port.h"

#include <QByteArray>
#include <QString>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for one calendar-event deletion. ApplicationServices
// remains caller-owned; only copied operation results cross the application
// boundary, never CalendarService or another legacy pointer.
class ApplicationServicesCalendarEventDeletePort final
    : public Application::CalendarEventDeletePort
{
public:
    explicit ApplicationServicesCalendarEventDeletePort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesCalendarEventDeletePort(
        const ApplicationServicesCalendarEventDeletePort&
        ) = delete;
    ApplicationServicesCalendarEventDeletePort& operator=(
        const ApplicationServicesCalendarEventDeletePort&
        ) = delete;
    ApplicationServicesCalendarEventDeletePort(
        ApplicationServicesCalendarEventDeletePort&&
        ) = delete;
    ApplicationServicesCalendarEventDeletePort& operator=(
        ApplicationServicesCalendarEventDeletePort&&
        ) = delete;

    [[nodiscard]] Application::CalendarEventDeleteResult deleteEvent(
        const Domain::CalendarEventId& eventId
        ) override
    {
        const auto legacyId = legacyEventId(eventId);
        if (!legacyId)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Calendar event identifier must be a positive integer."
                );
        }

        try
        {
            DatabaseSession* const session = m_services.databaseSession();
            if (!session || !session->isOpen())
            {
                return failure(
                    Domain::ErrorCode::NotFound,
                    "The calendar service is unavailable."
                    );
            }

            CalendarEventRepository* const repository =
                session->calendarEventRepository();
            if (!repository)
            {
                return failure(
                    Domain::ErrorCode::NotFound,
                    "The calendar event repository is unavailable."
                    );
            }

            const ::Status deleted =
                repository->deleteCalendarEvent(*legacyId);
            if (!deleted)
            {
                if (!session->isOpen())
                {
                    return failure(
                        Domain::ErrorCode::NotFound,
                        "The calendar service is unavailable."
                        );
                }

                const QByteArray errorBytes = deleted.error().toUtf8();
                return failure(
                    Domain::ErrorCode::Technical,
                    errorBytes.isEmpty()
                        ? "Calendar event could not be deleted."
                        : errorBytes.toStdString()
                    );
            }

            return Application::CalendarEventDeleteResult::success();
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Calendar event could not be deleted."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Calendar event could not be deleted."
                );
        }
    }

private:
    [[nodiscard]] static std::optional<int> legacyEventId(
        const Domain::CalendarEventId& eventId
        )
    {
        const std::string& value = eventId.value();
        if (value.empty())
        {
            return std::nullopt;
        }

        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        if (error != std::errc{} || end != value.data() + value.size())
        {
            return std::nullopt;
        }

        return parsed > 0
            ? std::optional<int>(parsed)
            : std::nullopt;
    }

    [[nodiscard]] static Application::CalendarEventDeleteResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return Application::CalendarEventDeleteResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
