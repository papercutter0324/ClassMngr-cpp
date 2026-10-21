#pragma once

#include "next/application/schedule_display_mode_preferences.h"

namespace ClassMngr::Next::Application
{

[[nodiscard]] constexpr bool scheduleDisplayModeUsesIntensiveSource(
    ScheduleDisplayMode mode
    ) noexcept
{
    return mode == ScheduleDisplayMode::Intensive;
}

} // namespace ClassMngr::Next::Application
