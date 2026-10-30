#pragma once

#include <memory>

namespace ClassMngr::Next::Bootstrap
{

// Kept injectable at the bootstrap boundary so startup sequencing can be
// observed without constructing the production network service.
class StartupUpdateDispatcher
{
public:
    virtual ~StartupUpdateDispatcher() = default;

    virtual void dispatch() = 0;

    // The bootstrap shell action uses the same owned update boundary even
    // when startup checks are disabled. No widget crosses this contract.
    virtual void openManual()
    {
    }

    // Cancel any owned asynchronous work before its dependencies are torn
    // down. Implementations without pending work may keep the default no-op.
    virtual void cancel() noexcept
    {
    }
};

[[nodiscard]] std::unique_ptr<StartupUpdateDispatcher>
makeStartupUpdateDispatcher();

}  // namespace ClassMngr::Next::Bootstrap
