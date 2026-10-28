#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/calendar_event_repository.h"
#include "next/application/calendar_event_delete_port.h"
#include "next/platform/repository_error_code.h"

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
        : m_store(services.workspaceStore())
    {
    }

    explicit ApplicationServicesCalendarEventDeletePort(
        WorkspaceStore& store
        ) noexcept
        : m_store(&store)
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
            WorkspaceStore* const store = m_store;
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

            const RepositoryStatus deleted =
                repository->deleteCalendarEvent(*legacyId);
            if (!deleted)
            {
                if (!store->isOpen())
                {
                    return unavailableStoreFailure();
                }

                const QByteArray errorBytes = deleted.error().diagnostic.toUtf8();
                return failure(
                    repositoryDomainErrorCode(deleted.error().code),
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
    [[nodiscard]] static Application::CalendarEventDeleteResult
    unavailableStoreFailure()
    {
        return Application::CalendarEventDeleteResult::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "The calendar service is unavailable.",
            .recoverable = true
        });
    }

    [[nodiscard]] static std::optional<qint64> legacyEventId(
        const Domain::CalendarEventId& eventId
        )
    {
        const std::string& value = eventId.value();
        if (value.empty())
        {
            return std::nullopt;
        }

        qint64 parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        if (error != std::errc{} || end != value.data() + value.size())
        {
            return std::nullopt;
        }

        return parsed > 0 && std::to_string(parsed) == value
            ? std::optional<qint64>(parsed)
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

    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform
