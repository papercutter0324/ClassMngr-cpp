#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/calendar_event_delete_all_port.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the calendar reset operation. The service remains
// behind ApplicationServices and only an owned structured result crosses out.
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
            const CalendarService* service = m_services.calendarService();
            return service && service->isAvailable();
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
            const CalendarService* service = m_services.calendarService();
            if (!service || !service->isAvailable())
            {
                return failure(
                    Domain::ErrorCode::NotFound,
                    "The calendar service is unavailable."
                    );
            }

            const ::Status deleted = service->deleteAllEvents();
            if (!deleted)
            {
                if (!service->isAvailable())
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
