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
};

[[nodiscard]] std::unique_ptr<StartupUpdateDispatcher>
makeStartupUpdateDispatcher();

}  // namespace ClassMngr::Next::Bootstrap
