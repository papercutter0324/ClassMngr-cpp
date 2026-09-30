#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/calendar_event_repository.h"
#include "domain/validation/calendar_event_validator.h"
#include "next/application/calendar_event_save_port.h"

#include <QByteArray>
#include <QDate>
#include <QString>
#include <QStringList>
#include <QTime>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for saving one non-repeat calendar event. The caller
// owns ApplicationServices; only the typed identifier/result crosses back to
// the application boundary.
class ApplicationServicesCalendarEventSavePort final
    : public Application::CalendarEventSavePort
{
public:
    explicit ApplicationServicesCalendarEventSavePort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesCalendarEventSavePort(
        const ApplicationServicesCalendarEventSavePort&
        ) = delete;
    ApplicationServicesCalendarEventSavePort& operator=(
        const ApplicationServicesCalendarEventSavePort&
        ) = delete;
    ApplicationServicesCalendarEventSavePort(
        ApplicationServicesCalendarEventSavePort&&
        ) = delete;
    ApplicationServicesCalendarEventSavePort& operator=(
        ApplicationServicesCalendarEventSavePort&&
        ) = delete;

    [[nodiscard]] Application::CalendarEventSaveResult saveEvent(
        const Application::CalendarEventSaveRequest& request
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
            const QDate startDate = legacyDate(request.startDate);
            const QDate endDate = legacyDate(request.endDate);
            if (!startDate.isValid()
                || !endDate.isValid()
                || endDate < startDate)
            {
                return failure(
                    Domain::ErrorCode::InvalidInput,
                    "Calendar event dates must be valid and ordered."
                    );
            }

            DatabaseSession* const session = m_services.databaseSession();
            if (!session || !session->isOpen())
            {
                return failure(
                    Domain::ErrorCode::NotFound,
                    "The calendar service is unavailable."
                    );
            }

            CalendarEvent event;
            if (request.id.has_value())
            {
                const auto legacyId = legacyEventId(*request.id);
                if (!legacyId)
                {
                    return failure(
                        Domain::ErrorCode::InvalidInput,
                        "Calendar event identifier must be a positive integer."
                        );
                }
                event.id = *legacyId;
            }

            event.title = legacyText(request.title);
            event.eventType = legacyText(request.eventType);
            event.timeStatus = legacyText(request.timeStatus);
            event.allDay = request.allDay;
            event.startDate = startDate;
            event.endDate = endDate;

            if (request.startTime.has_value())
            {
                const QTime startTime = legacyTime(*request.startTime);
                const QTime endTime = legacyTime(*request.endTime);
                if (!startTime.isValid() || !endTime.isValid())
                {
                    return failure(
                        Domain::ErrorCode::InvalidInput,
                        "Calendar event times must be valid."
                        );
                }
                event.startTime = startTime;
                event.endTime = endTime;
            }

            // This port is intentionally non-repeat. Repeat creation and
            // repeat editing remain on the existing batch-save path.
            event.repeatSeriesId.clear();

            const CalendarEvent normalized =
                CalendarEventValidator::normalized(event);
            const QList<CalendarEvent> normalizedEvents{normalized};
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
                        ? "Calendar event could not be saved."
                        : errorBytes.toStdString()
                    );
            }

            if (saved->size() != 1 || saved->front() <= 0)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    "Calendar event save did not return a valid identifier."
                    );
            }

            return Application::CalendarEventSaveResult::success(
                *Domain::CalendarEventId::fromString(
                    std::to_string(saved->front())
                    )
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Calendar event could not be saved."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Calendar event could not be saved."
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
        const QString date = legacyText(value);
        const QDate parsed = QDate::fromString(date, Qt::ISODate);
        return parsed.isValid()
                && parsed.toString(Qt::ISODate) == date
            ? parsed
            : QDate();
    }

    [[nodiscard]] static QTime legacyTime(
        const std::string& value
        )
    {
        const QString time = legacyText(value);
        const QTime parsed = QTime::fromString(
            time,
            QStringLiteral("HH:mm")
            );
        return parsed.isValid()
                && parsed.toString(QStringLiteral("HH:mm")) == time
            ? parsed
            : QTime();
    }

    [[nodiscard]] static std::optional<int> legacyEventId(
        const Domain::CalendarEventId& eventId
        )
    {
        const std::string& value = eventId.value();
        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        if (error != std::errc{}
            || end != value.data() + value.size()
            || parsed <= 0)
        {
            return std::nullopt;
        }

        return parsed;
    }

    [[nodiscard]] static Application::CalendarEventSaveResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return Application::CalendarEventSaveResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
