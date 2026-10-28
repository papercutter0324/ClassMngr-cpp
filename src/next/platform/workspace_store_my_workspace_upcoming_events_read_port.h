#pragma once

#include "data/database/workspace_store.h"
#include "data/repositories/calendar_event_repository.h"
#include "next/application/my_workspace_upcoming_events_read.h"
#include "next/platform/calendar_event_time_status_adapter.h"
#include "next/platform/repository_error_code.h"

#include <QByteArray>
#include <QDate>
#include <QString>

#include <exception>
#include <optional>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Bounded read-only adapter for the My Workspace Next 10 event summary.
class WorkspaceStoreMyWorkspaceUpcomingEventsReadPort final
    : public Application::MyWorkspaceUpcomingEventsReadPort
{
public:
    explicit WorkspaceStoreMyWorkspaceUpcomingEventsReadPort(
        WorkspaceStore& store
        ) noexcept
        : m_store(&store)
    {
    }

    [[nodiscard]] Application::MyWorkspaceUpcomingEventsReadResult
    readUpcomingEvents() const override
    {
        if (!m_store || !m_store->isOpen())
        {
            return unavailable();
        }
        CalendarEventRepository* const repository = m_store->calendarEventRepository();
        if (!repository)
        {
            return unavailable();
        }

        try
        {
            const RepositoryResult<QList<CalendarEvent>> loaded =
                repository->loadUpcomingCalendarEvents(
                    QDate::currentDate(),
                    static_cast<int>(Application::kMyWorkspaceUpcomingEventsMaximum)
                    );
            if (!loaded)
            {
                if (!m_store->isOpen())
                {
                    return unavailable();
                }
                const QByteArray diagnostic = loaded.error().diagnostic.toUtf8();
                std::string message = diagnostic.toStdString();
                if (message.empty())
                {
                    message = "Upcoming calendar events could not be loaded.";
                }
                return failure(
                    repositoryDomainErrorCode(loaded.error().code),
                    std::move(message)
                    );
            }
            if (static_cast<std::size_t>(loaded->size())
                > Application::kMyWorkspaceUpcomingEventsMaximum)
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "The calendar repository exceeded the ten-event summary limit."
                    );
            }

            Application::CalendarEventProjectionInput input;
            input.events.reserve(static_cast<std::size_t>(loaded->size()));
            for (qsizetype index = 0; index < loaded->size(); ++index)
            {
                const auto projected = projectEvent(
                    loaded->at(index),
                    Application::CalendarEventOrder(
                        static_cast<std::int32_t>(index)
                        )
                    );
                if (!projected)
                {
                    return failure(
                        projected.error().code,
                        projected.error().message
                        );
                }
                input.events.push_back(std::move(projected.value()));
            }

            const auto projection = Application::CalendarEventProjection::create(
                std::move(input)
                );
            if (!projection)
            {
                return failure(
                    projection.error().code,
                    projection.error().message
                    );
            }
            return Application::MyWorkspaceUpcomingEventsReadResult::success(
                std::move(projection.value())
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Upcoming calendar events could not be projected."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Upcoming calendar events could not be projected."
                );
        }
    }

private:
    [[nodiscard]] static std::optional<std::string> boundedUtf8(
        const QString& value,
        const std::size_t maximumBytes,
        const bool required
        )
    {
        const QString normalized = value.trimmed();
        if (normalized.isEmpty())
        {
            return required ? std::nullopt : std::optional<std::string>{std::string{}};
        }
        const QByteArray bytes = normalized.toUtf8();
        if (static_cast<std::size_t>(bytes.size()) > maximumBytes)
        {
            return std::nullopt;
        }
        return bytes.toStdString();
    }

    [[nodiscard]] static Domain::Result<Application::CalendarEventSummary>
    projectEvent(
        const CalendarEvent& source,
        const Application::CalendarEventOrder order
        )
    {
        if (source.id <= 0)
        {
            return Domain::Result<Application::CalendarEventSummary>::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "A calendar event has an invalid identifier.",
                .recoverable = false
            });
        }
        const auto id = Domain::CalendarEventId::fromString(
            std::to_string(source.id)
            );
        const auto title = boundedUtf8(
            source.title,
            Application::kCalendarEventSummaryMaxTitleUtf8Bytes,
            true
            );
        const auto startDate = boundedUtf8(
            source.startDate.toString(Qt::ISODate),
            Application::kCalendarEventSummaryMaxDateLength,
            true
            );
        const auto endDate = boundedUtf8(
            source.endDate.toString(Qt::ISODate),
            Application::kCalendarEventSummaryMaxDateLength,
            true
            );
        const auto eventType = boundedUtf8(
            normalizedCalendarEventType(source.eventType),
            Application::kCalendarEventSummaryMaxEventTypeLength,
            true
            );
        if (!id || !title || !startDate || !endDate || !eventType)
        {
            return Domain::Result<Application::CalendarEventSummary>::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "A calendar event contains invalid or oversized summary fields.",
                .recoverable = false
            });
        }

        std::optional<std::string> startTime;
        std::optional<std::string> endTime;
        if (!source.allDay)
        {
            const bool hasStart = source.startTime.isValid();
            const bool hasEnd = source.endTime.isValid();
            if (hasStart != hasEnd)
            {
                return Domain::Result<Application::CalendarEventSummary>::failure({
                    .code = Domain::ErrorCode::Validation,
                    .message = "A calendar event contains a partial time range.",
                    .recoverable = false
                });
            }
            if (hasStart)
            {
                startTime = source.startTime.toString(QStringLiteral("HH:mm"))
                    .toStdString();
                endTime = source.endTime.toString(QStringLiteral("HH:mm"))
                    .toStdString();
            }
        }

        Application::CalendarEventSummary summary{
            .id = *id,
            .classId = std::nullopt,
            .campusId = std::nullopt,
            .title = *title,
            .startDate = *startDate,
            .endDate = *endDate,
            .startTime = std::move(startTime),
            .endTime = std::move(endTime),
            .location = {},
            .notes = {},
            .order = order,
            .allDay = source.allDay,
            .eventType = Application::CalendarEventTypeName(*eventType),
            .timeStatus = calendarEventTimeStatusFromLegacyText(source.timeStatus),
            .repeatSeriesId = std::nullopt
        };
        const auto validation = Application::CalendarEventProjection::validate(summary);
        if (!validation)
        {
            return Domain::Result<Application::CalendarEventSummary>::failure(
                validation.error()
                );
        }
        return Domain::Result<Application::CalendarEventSummary>::success(
            std::move(summary)
            );
    }

    [[nodiscard]] static Application::MyWorkspaceUpcomingEventsReadResult
    unavailable()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The active workspace calendar repository is unavailable."
            );
    }

    [[nodiscard]] static Application::MyWorkspaceUpcomingEventsReadResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return Application::MyWorkspaceUpcomingEventsReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = true
        });
    }

    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform
