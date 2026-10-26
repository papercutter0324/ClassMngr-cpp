#pragma once

#include <functional>
#include <stdexcept>
#include <utility>

namespace ClassMngr::Next::Application
{

struct ShutdownResult final
{
    bool shutdownRequested = false;
    bool newlyRequested = false;
};

// Idempotent application shutdown boundary. The composition root supplies the
// concrete action that ends its event loop or releases its owned services.
class ShutdownCoordinator final
{
public:
    using ShutdownAction = std::function<void()>;

    explicit ShutdownCoordinator(ShutdownAction shutdownAction)
        : m_shutdownAction(std::move(shutdownAction))
    {
        if (!m_shutdownAction)
        {
            throw std::invalid_argument(
                "ShutdownCoordinator requires a shutdown action."
                );
        }
    }

    [[nodiscard]] ShutdownResult requestShutdown()
    {
        if (m_shutdownRequested)
        {
            return {true, false};
        }

        m_shutdownRequested = true;
        m_shutdownAction();
        return {true, true};
    }

    [[nodiscard]] bool shutdownRequested() const noexcept
    {
        return m_shutdownRequested;
    }

private:
    ShutdownAction m_shutdownAction;
    bool m_shutdownRequested = false;
};

}  // namespace ClassMngr::Next::Application
