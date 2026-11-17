#pragma once

#include "core/process_memory_snapshot.h"
#include "next/bootstrap/startup_diagnostics_sink.h"

#include <memory>

namespace ClassMngr::Next::Bootstrap
{

[[nodiscard]] std::unique_ptr<StartupDiagnosticsSink>
makeQtStartupDiagnosticsSink(
    std::unique_ptr<ProcessMemorySnapshotProvider> snapshotProvider = {}
    );

// Used until the normal sink exists, and if its construction fails.
void writeEarlyQtStartupDiagnostic(
    const Application::StartupEvent& event
    ) noexcept;

}  // namespace ClassMngr::Next::Bootstrap
