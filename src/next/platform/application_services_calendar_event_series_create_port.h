#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/calendar_event_repository.h"
#include "domain/validation/calendar_event_validator.h"
#include "next/application/calendar_event_series_create_port.h"

#include <QByteArray>
#include <QDate>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTime>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for creating a repeat series. The typed request is
// converted to one ordered batch and persisted through the repository's
// transaction; only copied typed identifiers/results return.
class ApplicationServicesCalendarEventSeriesCreatePort final
    : public Application::CalendarEventSeriesCreatePort
{
public:
    explicit ApplicationServicesCalendarEventSeriesCreatePort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesCalendarEventSeriesCreatePort(
        const ApplicationServicesCalendarEventSeriesCreatePort&
        ) = delete;
    ApplicationServicesCalendarEventSeriesCreatePort& operator=(
        const ApplicationServicesCalendarEventSeriesCreatePort&
        ) = delete;
    ApplicationServicesCalendarEventSeriesCreatePort(
        ApplicationServicesCalendarEventSeriesCreatePort&&
        ) = delete;
    ApplicationServicesCalendarEventSeriesCreatePort& operator=(
        ApplicationServicesCalendarEventSeriesCreatePort&&
        ) = delete;

    [[nodiscard]] Application::CalendarEventSeriesCreateResult
    createRepeatSeries(
        const Application::CalendarEventSeriesCreateRequest& request
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

            const QString repeatSeriesId = legacyText(
                request.repeatSeriesId
                ).trimmed();
            QList<CalendarEvent> normalizedEvents;
            normalizedEvents.reserve(
                static_cast<qsizetype>(request.occurrences.size())
                );
            for (const Application::CalendarEventSaveRequest& occurrence :
                 request.occurrences)
            {
                const auto legacyOccurrence = legacyEvent(
                    occurrence,
                    repeatSeriesId
                    );
                if (!legacyOccurrence)
                {
                    return failure(
                        Domain::ErrorCode::InvalidInput,
                        "Calendar repeat-series occurrence contains invalid "
                        "legacy date, time, or identifier values."
                        );
                }

                normalizedEvents.append(
                    CalendarEventValidator::normalized(*legacyOccurrence)
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

            // Keep this as the sole persistence call so the repository's
            // transaction covers the complete series and preserves order.
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
                        ? "Calendar repeat series could not be saved."
                        : errorBytes.toStdString()
                    );
            }

            if (saved->size()
                != static_cast<qsizetype>(request.occurrences.size()))
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    "Calendar repeat series save did not return one identifier "
                    "per occurrence."
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
                        "Calendar repeat series save returned an invalid "
                        "event identifier."
                        );
                }

                const auto typedId = Domain::CalendarEventId::fromString(
                    std::to_string(eventId)
                    );
                if (!typedId)
                {
                    return failure(
                        Domain::ErrorCode::Technical,
                        "Calendar repeat series save returned an invalid "
                        "event identifier."
                        );
                }

                eventIds.push_back(*typedId);
            }

            return Application::CalendarEventSeriesCreateResult::success(
                std::move(eventIds)
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Calendar repeat series could not be saved."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Calendar repeat series could not be saved."
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

    [[nodiscard]] static std::optional<CalendarEvent> legacyEvent(
        const Application::CalendarEventSaveRequest& request,
        const QString& repeatSeriesId
        )
    {
        const QDate startDate = legacyDate(request.startDate);
        const QDate endDate = legacyDate(request.endDate);
        if (!startDate.isValid()
            || !endDate.isValid()
            || endDate < startDate)
        {
            return std::nullopt;
        }

        CalendarEvent event;
        if (request.id.has_value())
        {
            const auto legacyId = legacyEventId(*request.id);
            if (!legacyId)
            {
                return std::nullopt;
            }
            event.id = *legacyId;
        }

        event.title = legacyText(request.title);
        event.eventType = legacyText(request.eventType);
        event.timeStatus = legacyText(request.timeStatus);
        event.repeatSeriesId = repeatSeriesId;
        event.allDay = request.allDay;
        event.startDate = startDate;
        event.endDate = endDate;

        if (request.startTime.has_value())
        {
            const QTime startTime = legacyTime(*request.startTime);
            const QTime endTime = legacyTime(*request.endTime);
            if (!startTime.isValid() || !endTime.isValid())
            {
                return std::nullopt;
            }

            event.startTime = startTime;
            event.endTime = endTime;
        }

        return event;
    }

    [[nodiscard]] static Application::CalendarEventSeriesCreateResult
    failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return Application::CalendarEventSeriesCreateResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
