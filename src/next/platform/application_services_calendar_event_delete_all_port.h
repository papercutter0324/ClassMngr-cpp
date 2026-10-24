#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/calendar_event_repository.h"
#include "next/application/calendar_event_delete_all_port.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for calendar reset. It resolves the active workspace
// store and repository through ApplicationServices and only an owned result
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
            WorkspaceStore* const store = m_services.workspaceStore();
            return store
                && store->isOpen()
                && store->calendarEventRepository() != nullptr;
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
            WorkspaceStore* const store = m_services.workspaceStore();
            if (!store || !store->isOpen())
            {
                return unavailableStoreFailure();
            }

            CalendarEventRepository* const repository =
                store->calendarEventRepository();
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
                if (!store->isOpen())
                {
                    return unavailableStoreFailure();
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
    [[nodiscard]] static Application::CalendarEventDeleteAllResult
    unavailableStoreFailure()
    {
        return Application::CalendarEventDeleteAllResult::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "The calendar service is unavailable.",
            .recoverable = true
        });
    }

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
