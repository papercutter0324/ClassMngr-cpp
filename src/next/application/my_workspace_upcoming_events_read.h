#pragma once

#include "next/application/calendar_event_projection.h"
#include "next/domain/operation_result.h"

#include <utility>

namespace ClassMngr::Next::Application
{

inline constexpr std::size_t kMyWorkspaceUpcomingEventsMaximum = 10;

using MyWorkspaceUpcomingEventsReadResult =
    Domain::Result<CalendarEventProjection>;

class MyWorkspaceUpcomingEventsReadPort
{
public:
    virtual ~MyWorkspaceUpcomingEventsReadPort() = default;

    [[nodiscard]] virtual MyWorkspaceUpcomingEventsReadResult
    readUpcomingEvents() const = 0;
};

// The overview intentionally displays only the legacy Next 10 summary.
// Calendar editing, filters, and month views stay with the full Calendar page.
class MyWorkspaceUpcomingEventsReadUseCase final
{
public:
    [[nodiscard]] static MyWorkspaceUpcomingEventsReadResult execute(
        const MyWorkspaceUpcomingEventsReadPort& port
        )
    {
        MyWorkspaceUpcomingEventsReadResult loaded = port.readUpcomingEvents();
        if (!loaded)
        {
            return MyWorkspaceUpcomingEventsReadResult::failure(loaded.error());
        }
        if (loaded.value().eventCount() > kMyWorkspaceUpcomingEventsMaximum)
        {
            return MyWorkspaceUpcomingEventsReadResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "Upcoming events exceed the ten-event summary limit.",
                .recoverable = false
            });
        }
        return MyWorkspaceUpcomingEventsReadResult::success(
            std::move(loaded.value())
            );
    }
};

} // namespace ClassMngr::Next::Application
