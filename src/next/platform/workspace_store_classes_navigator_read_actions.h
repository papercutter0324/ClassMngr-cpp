#pragma once

#include "next/application/classes_list_read_port.h"
#include "next/application/classes_navigation_snapshot.h"
#include "next/application/classes_navigator_preferences.h"

#include <QString>

namespace ClassMngr::Next::Platform
{

// Standalone path-scoped reads used by Next Classes. The page asks for a
// bounded class list first, then a separate bounded navigation snapshot.
[[nodiscard]] Application::ClassesListReadResult
readClassesNavigatorListAtPath(const QString& databasePath);

[[nodiscard]] Application::ClassesNavigationSnapshotResult
readClassesNavigatorNavigationAtPath(
    const QString& databasePath,
    const Application::ClassesNavigationSnapshotQuery& query
    );

[[nodiscard]] Application::ClassesNavigatorPreferences
readClassesNavigatorPreferencesAtPath(const QString& databasePath);

[[nodiscard]] Application::ScheduleDisplayModePreferencesSaveResult
saveClassesNavigatorScheduleModeAtPath(
    const QString& databasePath,
    Application::ScheduleDisplayMode mode
    );

[[nodiscard]] Application::ClassVisibilityPreferencesSaveResult
saveClassesNavigatorVisibilityAtPath(
    const QString& databasePath,
    Application::ClassVisibilityScope scope
    );

} // namespace ClassMngr::Next::Platform
