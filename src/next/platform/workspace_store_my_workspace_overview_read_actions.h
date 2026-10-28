#pragma once

#include "next/application/my_workspace_schedule_read.h"
#include "next/application/my_workspace_upcoming_events_read.h"

#include <QString>

namespace ClassMngr::Next::Platform
{

[[nodiscard]] Application::MyWorkspaceScheduleReadResult
readMyWorkspaceScheduleAtPath(const QString& databasePath);

[[nodiscard]] Application::MyWorkspaceUpcomingEventsReadResult
readMyWorkspaceUpcomingEventsAtPath(const QString& databasePath);

} // namespace ClassMngr::Next::Platform
