#pragma once

#include <cstdint>
#include <optional>

#include "next/application/schedule_slot_canonical_hour.h"

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
    std::optional<ScheduleSlotCanonicalHour> canonicalHour,
    bool useIntensiveTimes,
    bool regularWeekdayTogglingEnabled
    ) noexcept
{
    const ScheduleSlotState defaultState = canonicalWeekend
        ? ScheduleSlotState::Empty
        : (useIntensiveTimes
            ? ScheduleSlotState::Essay
            : (canonicalHour && canonicalHour->value() <= 15
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
