#include "next/ui/schedule_page_slot_policy_bridge.h"

#include "next/application/schedule_slot_canonical_hour.h"
#include "next/application/schedule_slot_policy.h"

namespace ClassMngr::Next::Ui
{

SchedulePageSlotPolicy schedulePageSlotPolicy(
    const bool weekend,
    const std::optional<int> canonicalHour,
    const bool intensive
    ) noexcept
{
    const std::optional<Application::ScheduleSlotCanonicalHour> hour =
        canonicalHour
            ? std::optional<Application::ScheduleSlotCanonicalHour>(
                  Application::ScheduleSlotCanonicalHour(*canonicalHour))
            : std::nullopt;
    const Application::ScheduleSlotPolicy policy =
        Application::scheduleSlotPolicy(weekend, hour, intensive, false);
    return {
        .defaultState = policy.defaultState
                == Application::ScheduleSlotState::Empty
            ? SchedulePageDefaultSlotState::Empty
            : SchedulePageDefaultSlotState::Essay,
        .togglingEnabled = policy.togglingEnabled
    };
}

} // namespace ClassMngr::Next::Ui
