#include "next/application/schedule_slot_policy.h"

#include <cstdlib>
#include <optional>

using ClassMngr::Next::Application::ScheduleSlotState;
using ClassMngr::Next::Application::scheduleSlotPolicy;

int main()
{
    const auto weekendPrecedence = scheduleSlotPolicy(
        true,
        9,
        true,
        false
        );
    if (weekendPrecedence.defaultState != ScheduleSlotState::Empty
        || !weekendPrecedence.togglingEnabled)
    {
        return EXIT_FAILURE;
    }

    const auto intensiveWeekday = scheduleSlotPolicy(
        false,
        9,
        true,
        false
        );
    if (intensiveWeekday.defaultState != ScheduleSlotState::Essay
        || !intensiveWeekday.togglingEnabled)
    {
        return EXIT_FAILURE;
    }

    const auto hour15 = scheduleSlotPolicy(
        false,
        15,
        false,
        false
        );
    const auto after15 = scheduleSlotPolicy(
        false,
        16,
        false,
        false
        );
    const auto missingHour = scheduleSlotPolicy(
        false,
        std::nullopt,
        false,
        false
        );
    if (hour15.defaultState != ScheduleSlotState::Empty
        || after15.defaultState != ScheduleSlotState::Essay
        || missingHour.defaultState != ScheduleSlotState::Essay)
    {
        return EXIT_FAILURE;
    }

    const auto regularWeekdayDisabled = scheduleSlotPolicy(
        false,
        9,
        false,
        false
        );
    const auto regularWeekdayEnabled = scheduleSlotPolicy(
        false,
        9,
        false,
        true
        );
    const auto regularWeekend = scheduleSlotPolicy(
        true,
        std::nullopt,
        false,
        false
        );
    if (regularWeekdayDisabled.togglingEnabled
        || !regularWeekdayEnabled.togglingEnabled
        || !regularWeekend.togglingEnabled)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
