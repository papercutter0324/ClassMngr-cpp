#pragma once

#include <functional>
#include <optional>
#include <stdexcept>
#include <utility>

namespace ClassMngr::Next::Application
{

enum class ShutdownFailureReason
{
    AlreadyInProgress,
    SettingsFlushFailed,
    WorkspaceCloseFailed,
    ActionFailed
};

struct ShutdownResult final
{
    bool shutdownRequested = false;
    bool newlyRequested = false;
    std::optional<ShutdownFailureReason> failure;
};

// Idempotent application shutdown boundary. The composition root supplies the
// retryable preparation action that settles shutdown prerequisites before
// final resource release. A failed action may be retried while the application
// remains alive.
class ShutdownCoordinator final
{
public:
    using ShutdownAction =
        std::function<std::optional<ShutdownFailureReason>()>;

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
            return {true, false, std::nullopt};
        }
        if (m_shutdownInProgress)
        {
            return {
                false,
                false,
                ShutdownFailureReason::AlreadyInProgress
            };
        }

        m_shutdownInProgress = true;
        std::optional<ShutdownFailureReason> failure;
        try
        {
            failure = m_shutdownAction();
        }
        catch (...)
        {
            failure = ShutdownFailureReason::ActionFailed;
        }
        m_shutdownInProgress = false;

        if (failure.has_value())
        {
            return {false, false, failure};
        }

        m_shutdownRequested = true;
        return {true, true, std::nullopt};
    }

    [[nodiscard]] bool shutdownRequested() const noexcept
    {
        return m_shutdownRequested;
    }

private:
    ShutdownAction m_shutdownAction;
    bool m_shutdownRequested = false;
    bool m_shutdownInProgress = false;
};

}  // namespace ClassMngr::Next::Application
