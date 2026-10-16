#pragma once

#include <cstdint>
#include <optional>

namespace ClassMngr::Next::Application
{

enum class ScheduleSlotState : std::uint8_t
{
    Empty,
    Essay
};

struct ScheduleSlotPolicy final
{
    ScheduleSlotState defaultState{ScheduleSlotState::Essay};
    bool togglingEnabled{false};
};

// The feature edge resolves raw day and time labels into these policy facts.
[[nodiscard]] constexpr ScheduleSlotPolicy scheduleSlotPolicy(
    bool canonicalWeekend,
    std::optional<int> canonicalHour,
    bool useIntensiveTimes,
    bool regularWeekdayTogglingEnabled
    ) noexcept
{
    const ScheduleSlotState defaultState = canonicalWeekend
        ? ScheduleSlotState::Empty
        : (useIntensiveTimes
            ? ScheduleSlotState::Essay
            : (canonicalHour && *canonicalHour <= 15
                ? ScheduleSlotState::Empty
                : ScheduleSlotState::Essay));

    return {
        defaultState,
        useIntensiveTimes
            || canonicalWeekend
            || regularWeekdayTogglingEnabled
    };
}

}
