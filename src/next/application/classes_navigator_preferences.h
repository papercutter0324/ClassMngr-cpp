#pragma once

#include "next/application/class_day_filter_reset_policy.h"
#include "next/application/class_selection_reset_policy.h"
#include "next/application/class_visibility_preferences.h"
#include "next/application/schedule_display_mode_preferences.h"

#include <optional>

namespace ClassMngr::Next::Application
{

// Preferences consumed by the lightweight Classes navigator. A missing value
// means its setting could not be read; the UI retains its current value.
struct ClassesNavigatorPreferences final
{
    std::optional<ScheduleDisplayMode> scheduleDisplayMode;
    std::optional<ClassVisibilityScope> visibilityScope;
    std::optional<ClassDayFilterResetPolicy> dayFilterResetPolicy;
    std::optional<ClassSelectionResetPolicy> classSelectionResetPolicy;
};

} // namespace ClassMngr::Next::Application
