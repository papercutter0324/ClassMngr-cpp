#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/calendar_event_repository.h"
#include "next/application/calendar_event_import_signature.h"
#include "next/application/calendar_event_import_signature_query_port.h"
#include "next/platform/repository_error_code.h"

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
            WorkspaceStore* const store = m_services.workspaceStore();
            return store
                && store->isOpen()
                && store->calendarEventRepository();
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
            WorkspaceStore* const store = m_services.workspaceStore();
            if (!store || !store->isOpen())
            {
                return unavailableSessionFailure();
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

            Application::CalendarEventImportSignatureKeys keys;
            const RepositoryStatus visited =
                repository->visitCalendarEventImportSignatureSourcesInRange(
                    startDate,
                    endDate,
                    [&keys](
                        const CalendarEventImportSignatureSourceRecord& record
                        )
                    {
                        keys.push_back(importSignatureKey(record));
                        return CalendarEventRangeVisitDecision::Continue;
                    }
                    );
            if (!visited)
            {
                const RepositoryError& repositoryError = visited.error();
                if (!store->isOpen())
                {
                    return unavailableSessionFailure();
                }

                return failure(
                    repositoryDomainErrorCode(repositoryError.code),
                    repositoryError.diagnostic.toUtf8().toStdString()
                    );
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
        const CalendarEventImportSignatureSourceRecord& record
        )
    {
        // Keep normalization and ISO date conversion at the Qt adapter edge.
        // The Application value intentionally excludes time and row metadata.
        return Application::CalendarEventImportSignature::
            fromNormalizedFields({
                .simplifiedTitle = record.title.simplified().toStdU16String(),
                .normalizedEventType = normalizedCalendarEventType(
                    record.eventType
                    ).toStdU16String(),
                .startDateIso = record.startDate
                    .toString(Qt::ISODate)
                    .toStdU16String(),
                .endDateIso = record.endDate
                    .toString(Qt::ISODate)
                    .toStdU16String(),
                .allDay = record.allDay,
                .normalizedTimeStatus = normalizedCalendarEventTimeStatus(
                    record.timeStatus
                    ).toStdU16String()
            });
    }

    [[nodiscard]] static Application::CalendarEventImportSignatureQueryResult
    unavailableSessionFailure()
    {
        return Application::CalendarEventImportSignatureQueryResult::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "The calendar event repository is unavailable.",
            .recoverable = true
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
