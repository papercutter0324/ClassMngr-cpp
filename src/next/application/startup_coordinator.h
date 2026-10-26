#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ClassMngr::Next::Application
{

enum class StartupStage : std::uint8_t
{
    ProcessCreated,
    CoreServicesCreated,
    SettingsLoaded,
    LanguageLoaded,
    ThemeLoaded,
    RequiredFontsLoaded,
    WorkspaceOpened,
    ShellCreated,
    InitialPageReady,
    StartupReady
};

enum class StartupStageOutcome : std::uint8_t
{
    Completed,
    Warning,
    Failed
};

enum class StartupStatus : std::uint8_t
{
    NotStarted,
    InProgress,
    InitialExperienceAvailable,
    StartupReady,
    Failed
};

enum class StartupFailureReason : std::uint8_t
{
    AlreadyStarted,
    InvalidTransition,
    DuplicateStage,
    BackwardStage,
    StartupReadyPrerequisitesIncomplete,
    InitialExperienceUnavailable,
    StageFailed
};

enum class StartupDiagnosticCode : std::uint8_t
{
    StageWarning,
    StageFailed,
    AlreadyStarted,
    InvalidTransition,
    DuplicateStage,
    BackwardStage,
    StartupReadyPrerequisitesIncomplete,
    InitialExperienceUnavailable
};

struct StartupDiagnostic final
{
    StartupDiagnosticCode code;
    std::optional<StartupStage> stage;
};

struct StartupEvent final
{
    StartupStage stage;
    StartupStageOutcome outcome = StartupStageOutcome::Completed;
    std::optional<StartupDiagnostic> diagnostic;
};

struct StartupResult final
{
    StartupStatus status = StartupStatus::NotStarted;
    // All accepted stage outcomes, including warning and failed outcomes.
    std::vector<StartupEvent> stageResults;
    // Compatibility view containing only stages with a Completed outcome.
    std::vector<StartupEvent> completedEvents;
    std::optional<StartupFailureReason> failure;
    std::optional<StartupDiagnostic> diagnostic;
    // Set only when this operation accepted an actual stage result.
    std::optional<StartupEvent> latestStageResult;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return (status == StartupStatus::InitialExperienceAvailable
                || status == StartupStatus::StartupReady)
            && !failure.has_value();
    }
};

// Tracks factual stage outcomes in the planned startup order. Owners may omit
// stages that are not part of their current composition, but accepted stages
// must move forward. StartupReady is gated on a recorded, non-failed outcome
// for every preceding stage; a warning still means that stage completed.
class StartupCoordinator final
{
public:
    [[nodiscard]] StartupResult begin()
    {
        if (m_started)
        {
            return failedResult(StartupFailureReason::AlreadyStarted);
        }

        m_started = true;
        return result();
    }

    [[nodiscard]] StartupResult recordStage(
        StartupStage stage,
        StartupStageOutcome outcome = StartupStageOutcome::Completed,
        std::optional<StartupFailureReason> failureReason = std::nullopt
        )
    {
        if (!m_started)
        {
            return failedResult(
                StartupFailureReason::InvalidTransition,
                stage
                );
        }

        const std::size_t stageIndex = indexOf(stage);
        if (stageIndex >= stageCount
            || (outcome != StartupStageOutcome::Completed
                && outcome != StartupStageOutcome::Warning
                && outcome != StartupStageOutcome::Failed))
        {
            return failedResult(StartupFailureReason::InvalidTransition);
        }

        if (m_hasStage[stageIndex])
        {
            return failedResult(StartupFailureReason::DuplicateStage, stage);
        }

        if (m_failure.has_value()
            || m_hasStage[indexOf(StartupStage::StartupReady)])
        {
            return failedResult(StartupFailureReason::InvalidTransition, stage);
        }

        if (m_lastStage.has_value() && stageIndex < indexOf(*m_lastStage))
        {
            return failedResult(StartupFailureReason::BackwardStage, stage);
        }

        if (m_lastStage.has_value() && stageIndex == indexOf(*m_lastStage))
        {
            return failedResult(StartupFailureReason::DuplicateStage, stage);
        }

        if (!m_lastStage.has_value() && stage != StartupStage::ProcessCreated)
        {
            return failedResult(StartupFailureReason::InvalidTransition, stage);
        }

        if (stage == StartupStage::InitialPageReady
            && !hasSuccessfulStage(StartupStage::ShellCreated))
        {
            return failedResult(StartupFailureReason::InvalidTransition, stage);
        }

        if (stage == StartupStage::StartupReady
            && !hasAllRequiredStagesCompleted())
        {
            return failedResult(
                StartupFailureReason::StartupReadyPrerequisitesIncomplete,
                stage
                );
        }

        if (outcome != StartupStageOutcome::Failed && failureReason.has_value())
        {
            return failedResult(StartupFailureReason::InvalidTransition, stage);
        }

        if (outcome == StartupStageOutcome::Failed && !failureReason.has_value())
        {
            failureReason = StartupFailureReason::StageFailed;
        }

        const std::optional<StartupDiagnostic> stageDiagnostic =
            outcome == StartupStageOutcome::Warning
                ? std::optional<StartupDiagnostic>{StartupDiagnostic{
                      StartupDiagnosticCode::StageWarning,
                      stage
                  }}
                : outcome == StartupStageOutcome::Failed
                    ? std::optional<StartupDiagnostic>{StartupDiagnostic{
                          diagnosticCodeFor(*failureReason),
                          stage
                      }}
                    : std::nullopt;

        const StartupEvent event{stage, outcome, stageDiagnostic};
        m_stageResults.push_back(event);
        m_hasStage[stageIndex] = true;
        m_stageOutcomes[stageIndex] = outcome;
        m_lastStage = stage;
        m_diagnostic = stageDiagnostic;

        if (outcome == StartupStageOutcome::Completed)
        {
            m_completedEvents.push_back(event);
        }
        else if (outcome == StartupStageOutcome::Failed)
        {
            m_failure = failureReason;
        }

        if (stage == StartupStage::StartupReady
            && outcome != StartupStageOutcome::Failed)
        {
            m_startupReady = true;
        }

        StartupResult accepted = result();
        accepted.latestStageResult = event;
        return accepted;
    }

    [[nodiscard]] StartupResult fail(StartupFailureReason reason)
    {
        if (!m_started || m_failure.has_value() || m_startupReady)
        {
            return failedResult(StartupFailureReason::InvalidTransition);
        }

        m_failure = reason;
        m_diagnostic = StartupDiagnostic{diagnosticCodeFor(reason), std::nullopt};
        return result();
    }

    [[nodiscard]] StartupResult result() const
    {
        StartupStatus status = StartupStatus::NotStarted;
        if (m_failure.has_value())
        {
            status = StartupStatus::Failed;
        }
        else if (m_startupReady)
        {
            status = StartupStatus::StartupReady;
        }
        else if (hasSuccessfulStage(StartupStage::InitialPageReady))
        {
            status = StartupStatus::InitialExperienceAvailable;
        }
        else if (m_started)
        {
            status = StartupStatus::InProgress;
        }

        return {
            status,
            m_stageResults,
            m_completedEvents,
            m_failure,
            m_diagnostic,
            std::nullopt
        };
    }

private:
    static constexpr std::size_t stageCount =
        static_cast<std::size_t>(StartupStage::StartupReady) + 1;

    [[nodiscard]] static constexpr std::size_t indexOf(StartupStage stage) noexcept
    {
        return static_cast<std::size_t>(stage);
    }

    [[nodiscard]] static constexpr StartupDiagnosticCode diagnosticCodeFor(
        StartupFailureReason reason
        ) noexcept
    {
        switch (reason)
        {
        case StartupFailureReason::AlreadyStarted:
            return StartupDiagnosticCode::AlreadyStarted;
        case StartupFailureReason::InvalidTransition:
            return StartupDiagnosticCode::InvalidTransition;
        case StartupFailureReason::DuplicateStage:
            return StartupDiagnosticCode::DuplicateStage;
        case StartupFailureReason::BackwardStage:
            return StartupDiagnosticCode::BackwardStage;
        case StartupFailureReason::StartupReadyPrerequisitesIncomplete:
            return StartupDiagnosticCode::StartupReadyPrerequisitesIncomplete;
        case StartupFailureReason::InitialExperienceUnavailable:
            return StartupDiagnosticCode::InitialExperienceUnavailable;
        case StartupFailureReason::StageFailed:
            return StartupDiagnosticCode::StageFailed;
        }

        return StartupDiagnosticCode::StageFailed;
    }

    [[nodiscard]] bool hasSuccessfulStage(StartupStage stage) const noexcept
    {
        const std::size_t stageIndex = indexOf(stage);
        return m_hasStage[stageIndex]
            && m_stageOutcomes[stageIndex] != StartupStageOutcome::Failed;
    }

    [[nodiscard]] bool hasAllRequiredStagesCompleted() const noexcept
    {
        for (std::size_t stageIndex = 0;
             stageIndex < indexOf(StartupStage::StartupReady);
             ++stageIndex)
        {
            if (!m_hasStage[stageIndex]
                || m_stageOutcomes[stageIndex] == StartupStageOutcome::Failed)
            {
                return false;
            }
        }

        return true;
    }

    [[nodiscard]] StartupResult failedResult(
        StartupFailureReason reason,
        std::optional<StartupStage> stage = std::nullopt
        ) const
    {
        StartupResult failed = result();
        failed.status = StartupStatus::Failed;
        failed.failure = reason;
        failed.diagnostic = StartupDiagnostic{diagnosticCodeFor(reason), stage};
        failed.latestStageResult = std::nullopt;
        return failed;
    }

    bool m_started = false;
    bool m_startupReady = false;
    std::array<bool, stageCount> m_hasStage{};
    std::array<StartupStageOutcome, stageCount> m_stageOutcomes{};
    std::vector<StartupEvent> m_stageResults;
    std::vector<StartupEvent> m_completedEvents;
    std::optional<StartupStage> m_lastStage;
    std::optional<StartupFailureReason> m_failure;
    std::optional<StartupDiagnostic> m_diagnostic;
};

}  // namespace ClassMngr::Next::Application
