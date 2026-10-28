#pragma once

#include "next/application/schedule_import_apply_request.h"
#include "next/application/schedule_import_state_snapshot.h"

#include <QString>

namespace ClassMngr::Next::Application
{
class ScheduleImportApplyUseCase;
}

namespace ClassMngr::Next::Platform
{

[[nodiscard]] Application::ScheduleImportStateSnapshotOutcome
readScheduleImportStateAtPath(const QString& databasePath);

[[nodiscard]] Application::ScheduleImportApplyResult
applyScheduleImportAtPath(
    const QString& databasePath,
    const Application::ScheduleImportApplyRequest& request
    );

} // namespace ClassMngr::Next::Platform
