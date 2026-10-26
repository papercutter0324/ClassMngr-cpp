#include "core/startup_profiler.h"

// WorkspaceStore repositories expose these legacy profiling hooks. Next does
// not activate the legacy startup profiler, so keep its workspace-only link
// closure explicit without importing the legacy profiler implementation.
void StartupProfiler::recordSubPrepRosterQuery(
    int,
    int,
    int,
    int,
    int
)
{
}

void StartupProfiler::recordScheduleImportApplyInputs(
    int,
    int,
    int
)
{
}

void StartupProfiler::recordScheduleImportApplyPrepared(
    int,
    int,
    int,
    int
)
{
}
