#include "core/startup_profiler.h"

// The Next output path preserves the legacy renderer's optional profiling
// hooks. Its runtime does not activate the legacy startup profiler, so these
// definitions keep the narrow renderer closure independent of Legacy Core.
bool StartupProfiler::isActive()
{
    return false;
}

void StartupProfiler::recordSubPrepOutputOperationStarted(const QString&) {}
void StartupProfiler::recordSubPrepOutputSourceProjected(int, int, int, qint64) {}
void StartupProfiler::recordSubPrepOutputPackageMapped(int, int, int, qint64, qint64) {}
void StartupProfiler::recordSubPrepOutputTypedSourceReleased() {}
void StartupProfiler::recordSubPrepOutputRendererProjected(int, int) {}
void StartupProfiler::recordSubPrepRenderSample(const QString&, const QJsonObject&) {}
void StartupProfiler::recordSubPrepOutputRendererProjectionReleased() {}
void StartupProfiler::recordSubPrepOutputPackageClassesReleased() {}
void StartupProfiler::recordSubPrepOutputGenerated(int, int, qint64, qint64) {}
void StartupProfiler::recordSubPrepOutputFailed(const QString&) {}
void StartupProfiler::recordSubPrepOutputOperationReleased() {}
