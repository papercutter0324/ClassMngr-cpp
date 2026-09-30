#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/calendar_event_repository.h"
#include "next/application/calendar_event_import_signature.h"
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

// Qt and repository adapter for the Application calendar-import signature
// query. It reads the complete date range instead of using the bounded event
// projection, preserving duplicate-detection semantics.
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
            DatabaseSession* const session = m_services.databaseSession();
            return session
                && session->isOpen()
                && session->calendarEventRepository();
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
            DatabaseSession* const session = m_services.databaseSession();
            if (!session || !session->isOpen())
            {
                return failure(
                    Domain::ErrorCode::NotFound,
                    "The calendar event repository is unavailable."
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

            const ::Result<QList<CalendarEvent>> loaded =
                repository->loadCalendarEventsInRange(startDate, endDate);
            if (!loaded)
            {
                const QString repositoryError = loaded.error();
                if (!session->isOpen())
                {
                    return failure(
                        Domain::ErrorCode::NotFound,
                        "The calendar event repository is unavailable."
                        );
                }

                return failure(
                    Domain::ErrorCode::Technical,
                    repositoryError.toUtf8().toStdString()
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
    [[nodiscard]] static Application::CalendarEventImportSignature
    importSignatureKey(
        const CalendarEvent& event
        )
    {
        // Keep normalization and ISO date conversion at the Qt adapter edge.
        // The Application value intentionally excludes time and row metadata.
        return Application::CalendarEventImportSignature::
            fromNormalizedFields({
                .simplifiedTitle = event.title.simplified().toStdU16String(),
                .normalizedEventType = normalizedCalendarEventType(
                    event.eventType
                    ).toStdU16String(),
                .startDateIso = event.startDate
                    .toString(Qt::ISODate)
                    .toStdU16String(),
                .endDateIso = event.endDate
                    .toString(Qt::ISODate)
                    .toStdU16String(),
                .allDay = event.allDay,
                .normalizedTimeStatus = normalizedCalendarEventTimeStatus(
                    event.timeStatus
                    ).toStdU16String()
            });
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
