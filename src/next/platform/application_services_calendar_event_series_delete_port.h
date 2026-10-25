#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/calendar_event_repository.h"
#include "next/application/calendar_event_series_delete_port.h"
#include "next/platform/repository_error_code.h"

#include <QByteArray>
#include <QDate>
#include <QString>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for deleting a repeat-series suffix. ApplicationServices
// remains caller-owned; only copied operation results cross the application
// boundary, never CalendarService or another legacy pointer.
class ApplicationServicesCalendarEventSeriesDeletePort final
    : public Application::CalendarEventSeriesDeletePort
{
public:
    explicit ApplicationServicesCalendarEventSeriesDeletePort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesCalendarEventSeriesDeletePort(
        const ApplicationServicesCalendarEventSeriesDeletePort&
        ) = delete;
    ApplicationServicesCalendarEventSeriesDeletePort& operator=(
        const ApplicationServicesCalendarEventSeriesDeletePort&
        ) = delete;
    ApplicationServicesCalendarEventSeriesDeletePort(
        ApplicationServicesCalendarEventSeriesDeletePort&&
        ) = delete;
    ApplicationServicesCalendarEventSeriesDeletePort& operator=(
        ApplicationServicesCalendarEventSeriesDeletePort&&
        ) = delete;

    [[nodiscard]] Application::CalendarEventSeriesDeleteResult
    deleteRepeatSeriesFromDate(
        const Application::CalendarEventSeriesDeleteRequest& request
        ) override
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return failure(
                validation.error().code,
                validation.error().message
                );
        }

        try
        {
            WorkspaceStore* const store = m_services.workspaceStore();
            if (!store || !store->isOpen())
            {
                return unavailableStoreFailure();
            }

            const QDate startDate(
                request.startDate.year(),
                request.startDate.month(),
                request.startDate.day()
                );
            const std::string& repeatSeriesIdValue =
                request.repeatSeriesId->value();
            const QString repeatSeriesId = QString::fromUtf8(
                repeatSeriesIdValue.data(),
                static_cast<qsizetype>(repeatSeriesIdValue.size())
                );
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
                repository->deleteCalendarEventsForRepeatSeriesFromDate(
                    repeatSeriesId,
                    startDate
                    );
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
                        ? "Calendar repeat series could not be deleted."
                        : errorBytes.toStdString()
                    );
            }

            return Application::CalendarEventSeriesDeleteResult::success();
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Calendar repeat series could not be deleted."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Calendar repeat series could not be deleted."
                );
        }
    }

private:
    [[nodiscard]] static Application::CalendarEventSeriesDeleteResult
    unavailableStoreFailure()
    {
        return Application::CalendarEventSeriesDeleteResult::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "The calendar service is unavailable.",
            .recoverable = true
        });
    }

    [[nodiscard]] static Application::CalendarEventSeriesDeleteResult
    failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return Application::CalendarEventSeriesDeleteResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
