#pragma once

#include "next/application/calendar_event_projection.h"
#include "next/application/calendar_event_start_of_term_policy.h"

#include <utility>

namespace ClassMngr::Next::Application
{

struct CalendarEventVisibilityPolicy final
{
    // The feature adapter supplies a lazy campus check after applying its
    // platform-specific title and campus-code normalization. Start-of-term
    // hiding is evaluated first, so show-all-campus settings cannot bypass it.
    template <typename CampusVisibilityCheck>
    [[nodiscard]] static bool shouldShowEvent(
        const CalendarEventSummary& event,
        const bool hideStartOfTermEvents,
        CampusVisibilityCheck&& campusVisibilityCheck
        )
    {
        if (CalendarEventStartOfTermPolicy::shouldHideEvent(
                event.title,
                event.eventType,
                hideStartOfTermEvents
                ))
        {
            return false;
        }

        return std::forward<CampusVisibilityCheck>(campusVisibilityCheck)();
    }
};

}
