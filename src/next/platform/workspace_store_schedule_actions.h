#pragma once

#include "next/application/schedule_page_read.h"
#include "next/application/schedule_slot_state_save.h"

#include <QString>

namespace ClassMngr::Next::Platform
{

// Every operation opens the selected .tps by its exact path. No process-wide
// active-store singleton or ApplicationServices route is involved.
[[nodiscard]] Application::SchedulePageReadResult readSchedulePageAtPath(
    const QString& databasePath
    );

[[nodiscard]] Application::ScheduleDisplayPreferencesSaveResult
saveScheduleDisplayPreferencesAtPath(
    const QString& databasePath,
    const Application::ScheduleDisplayPreferencesSaveRequest& request
    );

[[nodiscard]] Application::ScheduleDisplayModePreferencesSaveResult
saveScheduleDisplayModeAtPath(
    const QString& databasePath,
    Application::ScheduleDisplayMode mode
    );

[[nodiscard]] Application::ScheduleSlotStateSaveResult saveScheduleSlotStateAtPath(
    const QString& databasePath,
    const Application::ScheduleSlotStateSaveRequest& request
    );

} // namespace ClassMngr::Next::Platform
