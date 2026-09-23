#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/calendar_event_import_signature_query_port.h"

#include <QByteArray>
#include <QDate>
#include <QList>
#include <QString>

#include <cstddef>
#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt and legacy-service adapter for the Application calendar-import signature
// query. It intentionally reads the complete date range instead of using the
// bounded event projection, preserving legacy duplicate-detection semantics.
class ApplicationServicesCalendarEventImportSignatureQueryPort final
    : public Application::CalendarEventImportSignatureQueryPort
{
public:
    explicit ApplicationServicesCalendarEventImportSignatureQueryPort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesCalendarEventImportSignatureQueryPort(
        const ApplicationServicesCalendarEventImportSignatureQueryPort&
        ) = delete;
    ApplicationServicesCalendarEventImportSignatureQueryPort& operator=(
        const ApplicationServicesCalendarEventImportSignatureQueryPort&
        ) = delete;
    ApplicationServicesCalendarEventImportSignatureQueryPort(
        ApplicationServicesCalendarEventImportSignatureQueryPort&&
        ) = delete;
    ApplicationServicesCalendarEventImportSignatureQueryPort& operator=(
        ApplicationServicesCalendarEventImportSignatureQueryPort&&
        ) = delete;

    [[nodiscard]] bool isAvailable() const noexcept override
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

    [[nodiscard]] Application::CalendarEventImportSignatureQueryResult
    loadSignaturesInRange(
        const Application::CalendarEventImportSignatureRangeRequest& request
        ) const override
    {
        const QDate startDate = QDate::fromString(
            QString::fromStdString(request.startDate.value()),
            Qt::ISODate
            );
        const QDate endDate = QDate::fromString(
            QString::fromStdString(request.endDate.value()),
            Qt::ISODate
            );
        if (!request.startDate.isValid()
            || !request.endDate.isValid()
            || !startDate.isValid()
            || !endDate.isValid()
            || startDate.toString(Qt::ISODate).toStdString()
                != request.startDate.value()
            || endDate.toString(Qt::ISODate).toStdString()
                != request.endDate.value()
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
                    "No Teacher Profile service is available."
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
                        "No Teacher Profile service is available."
                        );
                }

                return failure(
                    Domain::ErrorCode::Technical,
                    legacyError.toUtf8().toStdString()
                    );
            }

            const QList<CalendarEvent>& events = loaded.value();
            Application::CalendarEventImportSignatureKeys keys;
            keys.reserve(static_cast<std::size_t>(events.size()));
            for (const CalendarEvent& event : events)
            {
                keys.push_back(importSignatureKey(event));
            }

            return Application::CalendarEventImportSignatureQueryResult::
                success(std::move(keys));
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Calendar event import signatures could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Calendar event import signatures could not be loaded."
                );
        }
    }

private:
    [[nodiscard]] static std::u16string importSignatureKey(
        const CalendarEvent& event
        )
    {
        // Keep the importer's established six-field QString identity and its
        // UTF-16 key representation. This read intentionally avoids the
        // bounded general event projection, whose capacity and metadata checks
        // are not part of legacy import duplicate detection.
        const QString signature = QStringLiteral("%1|%2|%3|%4|%5|%6")
            .arg(
                event.title.simplified(),
                normalizedCalendarEventType(event.eventType),
                event.startDate.toString(Qt::ISODate),
                event.endDate.toString(Qt::ISODate),
                event.allDay ? QStringLiteral("1") : QStringLiteral("0"),
                normalizedCalendarEventTimeStatus(event.timeStatus)
                );
        return signature.toStdU16String();
    }

    [[nodiscard]] static Application::CalendarEventImportSignatureQueryResult
    failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return Application::CalendarEventImportSignatureQueryResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
