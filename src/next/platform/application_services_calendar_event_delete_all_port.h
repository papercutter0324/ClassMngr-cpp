#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/calendar_event_repository.h"
#include "next/application/calendar_event_delete_all_port.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for calendar reset. It resolves the active session and
// repository through ApplicationServices and only an owned structured result
// crosses out.
class ApplicationServicesCalendarEventDeleteAllPort final
    : public Application::CalendarEventDeleteAllPort
{
public:
    explicit ApplicationServicesCalendarEventDeleteAllPort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesCalendarEventDeleteAllPort(
        const ApplicationServicesCalendarEventDeleteAllPort&
        ) = delete;
    ApplicationServicesCalendarEventDeleteAllPort& operator=(
        const ApplicationServicesCalendarEventDeleteAllPort&
        ) = delete;
    ApplicationServicesCalendarEventDeleteAllPort(
        ApplicationServicesCalendarEventDeleteAllPort&&
        ) = delete;
    ApplicationServicesCalendarEventDeleteAllPort& operator=(
        ApplicationServicesCalendarEventDeleteAllPort&&
        ) = delete;

    [[nodiscard]] bool isAvailable() const override
    {
        try
        {
            DatabaseSession* const session = m_services.databaseSession();
            return session
                && session->isOpen()
                && session->calendarEventRepository() != nullptr;
        }
        catch (...)
        {
            return false;
        }
    }

    [[nodiscard]] Application::CalendarEventDeleteAllResult
    deleteAllEvents() override
    {
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

            const ::Status deleted = repository->deleteAllCalendarEvents();
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
                        ? "Calendar events could not be reset."
                        : errorBytes.toStdString()
                    );
            }

            return Application::CalendarEventDeleteAllResult::success();
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Calendar events could not be reset."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Calendar events could not be reset."
                );
        }
    }

private:
    [[nodiscard]] static Application::CalendarEventDeleteAllResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return Application::CalendarEventDeleteAllResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
