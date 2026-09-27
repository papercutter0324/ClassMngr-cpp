#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/calendar_event_series_delete_port.h"

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
            const CalendarService* service = m_services.calendarService();
            if (!service || !service->isAvailable())
            {
                return failure(
                    Domain::ErrorCode::NotFound,
                    "The calendar service is unavailable."
                    );
            }

            const QDate startDate = QDate::fromString(
                QString::fromUtf8(
                    request.startDate.data(),
                    static_cast<qsizetype>(request.startDate.size())
                    ),
                Qt::ISODate
                );
            const QString repeatSeriesId = QString::fromUtf8(
                request.repeatSeriesId.data(),
                static_cast<qsizetype>(request.repeatSeriesId.size())
                );
            const ::Status deleted = service->deleteRepeatSeriesFromDate(
                repeatSeriesId,
                startDate
                );
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
