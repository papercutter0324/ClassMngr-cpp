#pragma once

#include "next/application/startup_coordinator.h"

namespace ClassMngr::Next::Bootstrap
{

// Process-level startup records contain only the coordinator's typed outcome.
// Concrete sinks belong to the composition edge so the application contracts
// stay independent of Qt and other runtime services.
class StartupDiagnosticsSink
{
public:
    virtual ~StartupDiagnosticsSink() = default;

    virtual void record(const Application::StartupEvent& event) noexcept = 0;
};

}  // namespace ClassMngr::Next::Bootstrap
