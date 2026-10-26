#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace ClassMngr::Next::Application
{

enum class StartupStage : std::uint8_t
{
    InitialExperienceAvailable
};

enum class StartupStatus : std::uint8_t
{
    NotStarted,
    InProgress,
    InitialExperienceAvailable,
    Failed
};

enum class StartupFailureReason : std::uint8_t
{
    AlreadyStarted,
    InvalidTransition,
    InitialExperienceUnavailable
};

struct StartupEvent final
{
    StartupStage stage;
};

struct StartupResult final
{
    StartupStatus status = StartupStatus::NotStarted;
    std::vector<StartupEvent> completedEvents;
    std::optional<StartupFailureReason> failure;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return status == StartupStatus::InitialExperienceAvailable
            && !failure.has_value();
    }
};

// Tracks only the startup milestone that this application slice actually
// completes. Later resource, preference, workspace, and runtime composition
// milestones are added when those owners are part of the bootstrap.
class StartupCoordinator final
{
public:
    [[nodiscard]] StartupResult begin()
    {
        if (m_status != StartupStatus::NotStarted)
        {
            return failedResult(StartupFailureReason::AlreadyStarted);
        }

        m_status = StartupStatus::InProgress;
        return result();
    }

    [[nodiscard]] StartupResult initialExperienceAvailable()
    {
        if (m_status != StartupStatus::InProgress)
        {
            return failedResult(StartupFailureReason::InvalidTransition);
        }

        m_completedEvents.push_back({StartupStage::InitialExperienceAvailable});
        m_status = StartupStatus::InitialExperienceAvailable;
        return result();
    }

    [[nodiscard]] StartupResult fail(StartupFailureReason reason)
    {
        if (m_status != StartupStatus::InProgress)
        {
            return failedResult(StartupFailureReason::InvalidTransition);
        }

        m_status = StartupStatus::Failed;
        m_failure = reason;
        return result();
    }

    [[nodiscard]] StartupResult result() const
    {
        return {m_status, m_completedEvents, m_failure};
    }

private:
    [[nodiscard]] StartupResult failedResult(StartupFailureReason reason) const
    {
        return {StartupStatus::Failed, m_completedEvents, reason};
    }

    StartupStatus m_status = StartupStatus::NotStarted;
    std::vector<StartupEvent> m_completedEvents;
    std::optional<StartupFailureReason> m_failure;
};

}  // namespace ClassMngr::Next::Application
