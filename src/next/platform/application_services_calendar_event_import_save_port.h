#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/calendar_event_repository.h"
#include "domain/validation/calendar_event_validator.h"
#include "next/application/calendar_event_import_save_port.h"

#include <QByteArray>
#include <QDate>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTime>

#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

// Converts an ordered, typed import batch and makes one repository call so
// its transaction covers the complete import.
class ApplicationServicesCalendarEventImportSavePort final
    : public Application::CalendarEventImportSavePort
{
public:
    explicit ApplicationServicesCalendarEventImportSavePort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesCalendarEventImportSavePort(
        const ApplicationServicesCalendarEventImportSavePort&
        ) = delete;
    ApplicationServicesCalendarEventImportSavePort& operator=(
        const ApplicationServicesCalendarEventImportSavePort&
        ) = delete;
    ApplicationServicesCalendarEventImportSavePort(
        ApplicationServicesCalendarEventImportSavePort&&
        ) = delete;
    ApplicationServicesCalendarEventImportSavePort& operator=(
        ApplicationServicesCalendarEventImportSavePort&&
        ) = delete;

    [[nodiscard]] Application::CalendarEventImportSaveResult
    saveImportedEvents(
        const Application::CalendarEventImportSaveRequest& request
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
            DatabaseSession* const session = m_services.databaseSession();
            if (!session || !session->isOpen())
            {
                return failure(
                    Domain::ErrorCode::NotFound,
                    "The calendar service is unavailable."
                    );
            }

            QList<CalendarEvent> normalizedEvents;
            normalizedEvents.reserve(
                static_cast<qsizetype>(request.events.size())
                );
            for (const Application::CalendarEventSaveRequest& source :
                 request.events)
            {
                const auto event = legacyEvent(source);
                if (!event)
                {
                    return failure(
                        Domain::ErrorCode::InvalidInput,
                        "Calendar import event contains invalid date or time values."
                        );
                }

                normalizedEvents.append(
                    CalendarEventValidator::normalized(*event)
                    );
            }

            const ValidationResult validation =
                CalendarEventValidator::validateSeries(normalizedEvents);
            if (validation.hasErrors())
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    validationError(validation)
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

            // Keep one save call: CalendarEventRepository owns the batch
            // transaction and preserves input order, including empty batches.
            const ::Result<QList<int>> saved =
                repository->saveCalendarEvents(normalizedEvents);
            if (!saved)
            {
                if (!session->isOpen())
                {
                    return failure(
                        Domain::ErrorCode::NotFound,
                        "The calendar service is unavailable."
                        );
                }

                const QByteArray errorBytes = saved.error().toUtf8();
                return failure(
                    Domain::ErrorCode::Technical,
                    errorBytes.isEmpty()
                        ? "Calendar import events could not be saved."
                        : errorBytes.toStdString()
                    );
            }

            if (saved->size()
                != static_cast<qsizetype>(request.events.size()))
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    "Calendar import save did not return one identifier per event."
                    );
            }

            std::vector<Domain::CalendarEventId> eventIds;
            eventIds.reserve(static_cast<std::size_t>(saved->size()));
            for (const int eventId : *saved)
            {
                if (eventId <= 0)
                {
                    return failure(
                        Domain::ErrorCode::Technical,
                        "Calendar import save returned an invalid event identifier."
                        );
                }

                const auto typedId = Domain::CalendarEventId::fromString(
                    std::to_string(eventId)
                    );
                if (!typedId)
                {
                    return failure(
                        Domain::ErrorCode::Technical,
                        "Calendar import save returned an invalid event identifier."
                        );
                }

                eventIds.push_back(*typedId);
            }

            return Application::CalendarEventImportSaveResult::success(
                std::move(eventIds)
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Calendar import events could not be saved."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Calendar import events could not be saved."
                );
        }
    }

private:
    [[nodiscard]] static std::string validationError(
        const ValidationResult& validation
        )
    {
        QStringList details;
        for (const ValidationIssue& issue : validation.errors())
        {
            QString detail = issue.field.isEmpty()
                ? issue.code
                : QStringLiteral("%1: %2").arg(issue.field, issue.code);
            if (issue.row >= 0 && !issue.field.contains(QChar(u'[')))
            {
                detail.prepend(QStringLiteral("row %1, ").arg(issue.row + 1));
            }
            details.append(detail);
        }

        return QStringLiteral("Calendar events validation failed: %1")
            .arg(details.join(QStringLiteral("; ")))
            .toStdString();
    }

    [[nodiscard]] static QString legacyText(
        const std::string& value
        )
    {
        return QString::fromUtf8(
            value.data(),
            static_cast<qsizetype>(value.size())
            );
    }

    [[nodiscard]] static QDate legacyDate(
        const std::string& value
        )
    {
        const QString text = legacyText(value);
        const QDate date = QDate::fromString(text, Qt::ISODate);
        return date.isValid() && date.toString(Qt::ISODate) == text
            ? date
            : QDate();
    }

    [[nodiscard]] static QTime legacyTime(
        const std::string& value
        )
    {
        const QString text = legacyText(value);
        const QTime time = QTime::fromString(
            text,
            QStringLiteral("HH:mm")
            );
        return time.isValid()
                && time.toString(QStringLiteral("HH:mm")) == text
            ? time
            : QTime();
    }

    [[nodiscard]] static std::optional<CalendarEvent> legacyEvent(
        const Application::CalendarEventSaveRequest& request
        )
    {
        if (request.id.has_value())
        {
            return std::nullopt;
        }

        const QDate startDate = legacyDate(request.startDate);
        const QDate endDate = legacyDate(request.endDate);
        if (!startDate.isValid() || !endDate.isValid() || endDate < startDate)
        {
            return std::nullopt;
        }

        CalendarEvent event;
        event.title = legacyText(request.title);
        event.eventType = legacyText(request.eventType);
        event.timeStatus = legacyText(request.timeStatus);
        event.allDay = request.allDay;
        event.startDate = startDate;
        event.endDate = endDate;

        if (request.startTime.has_value())
        {
            if (!request.endTime.has_value())
            {
                return std::nullopt;
            }

            const QTime startTime = legacyTime(*request.startTime);
            const QTime endTime = legacyTime(*request.endTime);
            if (!startTime.isValid() || !endTime.isValid())
            {
                return std::nullopt;
            }

            event.startTime = startTime;
            event.endTime = endTime;
        }

        event.repeatSeriesId.clear();
        return event;
    }

    [[nodiscard]] static Application::CalendarEventImportSaveResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return Application::CalendarEventImportSaveResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
