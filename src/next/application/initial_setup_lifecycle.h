#pragma once

#include "next/application/workspace_contracts.h"

#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

// Platform boundary for replacing a profile during the first-run setup flow.
// File operations must be non-destructive: moveProfile must not replace an
// existing destination, and failed operations must leave their source intact.
class InitialSetupLifecyclePort
{
public:
    virtual ~InitialSetupLifecyclePort() = default;

    [[nodiscard]] virtual bool hasActiveWorkspace() const = 0;

    [[nodiscard]] virtual Domain::Result<void> closeActiveWorkspace() = 0;

    [[nodiscard]] virtual bool profileExists(
        const WorkspaceLocation& location
        ) const = 0;

    [[nodiscard]] virtual WorkspaceLocation recoveryLocationFor(
        const WorkspaceLocation& profile
        ) = 0;

    [[nodiscard]] virtual bool moveProfile(
        const WorkspaceLocation& source,
        const WorkspaceLocation& destination
        ) = 0;

    [[nodiscard]] virtual bool removeProfile(
        const WorkspaceLocation& location
        ) = 0;

    [[nodiscard]] virtual Domain::Result<WorkspaceSession> createWorkspace(
        const CreateWorkspaceRequest& request
        ) = 0;
};

enum class InitialSetupBeginFailure
{
    None,
    AlreadyActive,
    CloseWorkspace,
    PreserveProfile,
    CreateWorkspace
};

enum class InitialSetupCancelStatus
{
    NotActive,
    Canceled,
    CloseWorkspaceFailed,
    IncompleteProfileRemovalFailed,
    OriginalProfileRestoreFailed
};

struct InitialSetupCancelResult final
{
    InitialSetupCancelStatus status = InitialSetupCancelStatus::NotActive;
    bool workspaceWasOpen = false;
    bool workspaceClosed = false;
    WorkspaceLocation profile;
    WorkspaceLocation originalBackup;
    WorkspaceLocation incompleteRecovery;
};

struct InitialSetupBeginResult final
{
    InitialSetupBeginFailure failure = InitialSetupBeginFailure::None;
    bool workspaceWasOpen = false;
    bool workspaceClosed = false;
    WorkspaceLocation profile;
    WorkspaceLocation originalBackup;
    WorkspaceLocation createdProfile;
    Domain::OperationError error;
    InitialSetupCancelResult rollback;

    [[nodiscard]] bool started() const noexcept
    {
        return failure == InitialSetupBeginFailure::None;
    }
};

struct InitialSetupFinishResult final
{
    bool wasActive = false;
    bool originalBackupRemovalFailed = false;
    WorkspaceLocation originalBackup;
};

// Synchronous orchestration for the profile replacement performed during
// initial setup. It retains only the pending replacement ticket; workspace
// session ownership remains with the existing coordinator.
class InitialSetupLifecycle final
{
public:
    explicit InitialSetupLifecycle(
        InitialSetupLifecyclePort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] InitialSetupBeginResult begin(
        WorkspaceLocation profile
        )
    {
        InitialSetupBeginResult result;
        result.profile = profile;

        if (m_hasPendingSetup)
        {
            result.failure = InitialSetupBeginFailure::AlreadyActive;
            result.error = conflict(
                "An initial setup profile replacement is already active."
                );
            return result;
        }

        result.workspaceWasOpen = m_port.hasActiveWorkspace();
        const auto closed = m_port.closeActiveWorkspace();
        if (!closed)
        {
            result.failure = InitialSetupBeginFailure::CloseWorkspace;
            result.error = closed.error();
            return result;
        }
        result.workspaceClosed = true;

        m_profile = std::move(profile);
        m_originalBackup = WorkspaceLocation{};

        if (m_port.profileExists(m_profile))
        {
            m_originalBackup = m_port.recoveryLocationFor(m_profile);
            if (
                m_originalBackup.empty()
                || !m_port.moveProfile(m_profile, m_originalBackup)
                )
            {
                result.failure = InitialSetupBeginFailure::PreserveProfile;
                result.originalBackup = m_originalBackup;
                result.error = technicalFailure(
                    "The existing profile could not be preserved."
                    );
                m_profile = WorkspaceLocation{};
                m_originalBackup = WorkspaceLocation{};
                return result;
            }
        }

        m_hasPendingSetup = true;
        result.originalBackup = m_originalBackup;

        const auto created = m_port.createWorkspace(
            CreateWorkspaceRequest{m_profile}
            );
        if (!created)
        {
            result.failure = InitialSetupBeginFailure::CreateWorkspace;
            result.error = created.error();
            result.rollback = cancel();
            return result;
        }

        result.createdProfile = created.value().location();
        return result;
    }

    [[nodiscard]] InitialSetupFinishResult finish()
    {
        InitialSetupFinishResult result;
        if (!m_hasPendingSetup)
        {
            return result;
        }

        result.wasActive = true;
        result.originalBackup = m_originalBackup;
        if (
            !m_originalBackup.empty()
            && !m_port.removeProfile(m_originalBackup)
            )
        {
            result.originalBackupRemovalFailed = true;
        }

        clearPendingSetup();
        return result;
    }

    [[nodiscard]] InitialSetupCancelResult cancel()
    {
        InitialSetupCancelResult result;
        if (!m_hasPendingSetup)
        {
            return result;
        }

        result.profile = m_profile;
        result.originalBackup = m_originalBackup;
        result.workspaceWasOpen = m_port.hasActiveWorkspace();
        const auto closed = m_port.closeActiveWorkspace();
        if (!closed)
        {
            result.status = InitialSetupCancelStatus::CloseWorkspaceFailed;
            return result;
        }
        result.workspaceClosed = true;

        if (m_originalBackup.empty())
        {
            if (
                m_port.profileExists(m_profile)
                && !m_port.removeProfile(m_profile)
                )
            {
                result.status =
                    InitialSetupCancelStatus::IncompleteProfileRemovalFailed;
                clearPendingSetup();
                return result;
            }

            result.status = InitialSetupCancelStatus::Canceled;
            clearPendingSetup();
            return result;
        }

        bool movedIncompleteProfile = true;
        if (m_port.profileExists(m_profile))
        {
            result.incompleteRecovery = m_port.recoveryLocationFor(m_profile);
            movedIncompleteProfile =
                !result.incompleteRecovery.empty()
                && m_port.moveProfile(m_profile, result.incompleteRecovery);
        }

        if (
            !movedIncompleteProfile
            || !m_port.moveProfile(m_originalBackup, m_profile)
            )
        {
            if (
                movedIncompleteProfile
                && !result.incompleteRecovery.empty()
                && m_port.profileExists(result.incompleteRecovery)
                && !m_port.moveProfile(result.incompleteRecovery, m_profile)
                )
            {
                // Keep the incomplete profile at its recovery path if putting
                // it back fails. The original remains at originalBackup.
            }

            result.status =
                InitialSetupCancelStatus::OriginalProfileRestoreFailed;
            clearPendingSetup();
            return result;
        }

        if (
            !result.incompleteRecovery.empty()
            && m_port.profileExists(result.incompleteRecovery)
            )
        {
            // The original profile is already restored. A failed cleanup only
            // leaves an extra recoverable setup artifact.
            (void) m_port.removeProfile(result.incompleteRecovery);
        }

        result.status = InitialSetupCancelStatus::Canceled;
        clearPendingSetup();
        return result;
    }

    [[nodiscard]] bool hasPendingSetup() const noexcept
    {
        return m_hasPendingSetup;
    }

private:
    [[nodiscard]] static Domain::OperationError conflict(
        std::string message
        )
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Conflict,
            .message = std::move(message),
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError technicalFailure(
        std::string message
        )
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Technical,
            .message = std::move(message),
            .recoverable = true
        };
    }

    void clearPendingSetup() noexcept
    {
        m_hasPendingSetup = false;
        m_profile = WorkspaceLocation{};
        m_originalBackup = WorkspaceLocation{};
    }

    InitialSetupLifecyclePort& m_port;
    bool m_hasPendingSetup = false;
    WorkspaceLocation m_profile;
    WorkspaceLocation m_originalBackup;
};

} // namespace ClassMngr::Next::Application
