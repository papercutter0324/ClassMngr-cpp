#include "next/application/schedule_slot_policy.h"

#include <cstdlib>
#include <optional>
#include <type_traits>

using ClassMngr::Next::Application::ScheduleSlotCanonicalHour;
using ClassMngr::Next::Application::ScheduleSlotState;
using ClassMngr::Next::Application::scheduleSlotPolicy;

static_assert(std::is_constructible_v<ScheduleSlotCanonicalHour, int>);
static_assert(!std::is_convertible_v<int, ScheduleSlotCanonicalHour>);
static_assert(!std::is_convertible_v<ScheduleSlotCanonicalHour, int>);
static_assert(std::is_same_v<
    decltype(ScheduleSlotCanonicalHour{0}.value()),
    int
    >);

int main()
{
    const ScheduleSlotCanonicalHour negativeHour{-23};
    const ScheduleSlotCanonicalHour equalHour{-23};
    const ScheduleSlotCanonicalHour differentHour{23};
    if (negativeHour.value() != -23
        || negativeHour != equalHour
        || negativeHour == differentHour)
    {
        return EXIT_FAILURE;
    }

    const auto weekendPrecedence = scheduleSlotPolicy(
        true,
        ScheduleSlotCanonicalHour{9},
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
        ScheduleSlotCanonicalHour{9},
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
        ScheduleSlotCanonicalHour{15},
        false,
        false
        );
    const auto after15 = scheduleSlotPolicy(
        false,
        ScheduleSlotCanonicalHour{16},
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
        ScheduleSlotCanonicalHour{9},
        false,
        false
        );
    const auto regularWeekdayEnabled = scheduleSlotPolicy(
        false,
        ScheduleSlotCanonicalHour{9},
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
