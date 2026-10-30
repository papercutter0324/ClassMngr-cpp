#pragma once

#include "next/application/schedule_page_read.h"
#include "next/application/schedule_slot_state_save.h"
#include "next/application/schedule_testing_assignment_save.h"
#include "next/application/personal_display_name_preferences.h"

#include <QString>

class WorkspaceStore;

namespace ClassMngr::Next::Platform
{

// Every operation opens the selected .tps by its exact path. No process-wide
// active-store singleton or ApplicationServices route is involved.
[[nodiscard]] Application::SchedulePageReadResult readSchedulePageFromStore(
    WorkspaceStore& store
    );
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

[[nodiscard]] Application::ScheduleTestingAssignmentSaveResult
saveScheduleTestingAssignmentAtPath(
    const QString& databasePath,
    const Application::ScheduleTestingAssignmentSaveRequest& request
    );

[[nodiscard]] Application::PersonalDisplayNamePreferencesReadResult
readScheduleOutputProfileNameAtPath(const QString& databasePath);

} // namespace ClassMngr::Next::Platform
