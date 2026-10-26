#pragma once

#include "next/bootstrap/startup_diagnostics_sink.h"

#include <memory>

namespace ClassMngr::Next::Bootstrap
{

[[nodiscard]] std::unique_ptr<StartupDiagnosticsSink>
makeQtStartupDiagnosticsSink();

// Used until the normal sink exists, and if its construction fails.
void writeEarlyQtStartupDiagnostic(
    const Application::StartupEvent& event
    ) noexcept;

}  // namespace ClassMngr::Next::Bootstrap
