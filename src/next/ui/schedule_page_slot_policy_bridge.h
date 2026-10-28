#pragma once

#include <optional>

namespace ClassMngr::Next::Ui
{

enum class SchedulePageDefaultSlotState
{
    Empty,
    Essay
};

struct SchedulePageSlotPolicy final
{
    SchedulePageDefaultSlotState defaultState =
        SchedulePageDefaultSlotState::Essay;
    bool togglingEnabled = false;
};

[[nodiscard]] SchedulePageSlotPolicy schedulePageSlotPolicy(
    bool weekend,
    std::optional<int> canonicalHour,
    bool intensive
    ) noexcept;

} // namespace ClassMngr::Next::Ui
