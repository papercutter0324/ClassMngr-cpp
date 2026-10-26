#pragma once

#include "next/application/workspace_contracts.h"

#include <string>
#include <optional>
#include <utility>

namespace ClassMngr::Next::Application
{

struct InitialSetupRecoveryRecord final
{
    WorkspaceLocation profile;
    WorkspaceLocation originalBackup;

    friend bool operator==(
        const InitialSetupRecoveryRecord&,
        const InitialSetupRecoveryRecord&
        ) = default;
};

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

    // Durable recovery is optional for adapters that only support the legacy
    // in-process setup flow. The v2 store adapter persists this record before
    // it moves an existing profile.
    [[nodiscard]] virtual Domain::Result<WorkspaceSession> openWorkspace(
        const OpenWorkspaceRequest& request
        )
    {
        static_cast<void>(request);
        return Domain::Result<WorkspaceSession>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Workspace recovery is not available through this adapter.",
            .recoverable = false
        });
    }

    [[nodiscard]] virtual Domain::Result<
        std::optional<InitialSetupRecoveryRecord>> pendingRecovery() const
    {
        return Domain::Result<
            std::optional<InitialSetupRecoveryRecord>>::success(
                std::nullopt
                );
    }

    [[nodiscard]] virtual bool recordPendingRecovery(
        const InitialSetupRecoveryRecord& record
        )
    {
        static_cast<void>(record);
        return true;
    }

    virtual void clearPendingRecovery(
        const InitialSetupRecoveryRecord& record
        ) noexcept
    {
        static_cast<void>(record);
    }
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
    OriginalRestoredRecoveryRemovalFailed,
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
            if (m_originalBackup.empty())
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

        if (!m_port.recordPendingRecovery(
                InitialSetupRecoveryRecord{m_profile, m_originalBackup}
                ))
        {
            result.failure = InitialSetupBeginFailure::PreserveProfile;
            result.originalBackup = m_originalBackup;
            result.error = technicalFailure(
                "The setup recovery record could not be saved."
                );
            m_profile = WorkspaceLocation{};
            m_originalBackup = WorkspaceLocation{};
            return result;
        }

        if (
            !m_originalBackup.empty()
            && !m_port.moveProfile(m_profile, m_originalBackup)
            )
        {
            m_port.clearPendingRecovery(
                InitialSetupRecoveryRecord{m_profile, m_originalBackup}
                );
            result.failure = InitialSetupBeginFailure::PreserveProfile;
            result.originalBackup = m_originalBackup;
            result.error = technicalFailure(
                "The existing profile could not be preserved."
                );
            m_profile = WorkspaceLocation{};
            m_originalBackup = WorkspaceLocation{};
            return result;
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

        if (!result.originalBackupRemovalFailed)
        {
            m_port.clearPendingRecovery(
                InitialSetupRecoveryRecord{m_profile, m_originalBackup}
                );
        }

        clearPendingSetup();
        return result;
    }

    // Reattach to an interrupted setup without deleting the original backup.
    // A missing target is recreated; an existing target must pass the normal
    // store-backed open validation before it becomes active.
    [[nodiscard]] InitialSetupBeginResult resume(
        const InitialSetupRecoveryRecord& recovery
        )
    {
        InitialSetupBeginResult result;
        result.profile = recovery.profile;
        result.originalBackup = recovery.originalBackup;
        if (m_hasPendingSetup)
        {
            result.failure = InitialSetupBeginFailure::AlreadyActive;
            result.error = conflict(
                "An initial setup profile replacement is already active."
                );
            return result;
        }
        if (recovery.profile.empty())
        {
            result.failure = InitialSetupBeginFailure::CreateWorkspace;
            result.error = technicalFailure(
                "The setup recovery record does not contain a profile path."
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

        m_profile = recovery.profile;
        m_originalBackup = recovery.originalBackup;
        m_hasPendingSetup = true;

        const auto resumed = m_port.profileExists(m_profile)
            ? m_port.openWorkspace(OpenWorkspaceRequest{m_profile})
            : m_port.createWorkspace(CreateWorkspaceRequest{m_profile});
        if (!resumed)
        {
            result.failure = InitialSetupBeginFailure::CreateWorkspace;
            result.error = resumed.error();
            clearPendingSetup();
            return result;
        }

        result.createdProfile = resumed.value().location();
        return result;
    }

    // Restore is safe even when an interrupted target is not a valid database:
    // cancel only uses Platform file operations and keeps both sides recoverable.
    [[nodiscard]] InitialSetupCancelResult restore(
        const InitialSetupRecoveryRecord& recovery
        )
    {
        if (m_hasPendingSetup)
        {
            if (m_profile != recovery.profile
                || m_originalBackup != recovery.originalBackup)
            {
                return InitialSetupCancelResult{};
            }
        }
        else
        {
            if (recovery.profile.empty())
            {
                return InitialSetupCancelResult{};
            }
            m_profile = recovery.profile;
            m_originalBackup = recovery.originalBackup;
            m_hasPendingSetup = true;
        }
        return cancel();
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

            m_port.clearPendingRecovery(
                InitialSetupRecoveryRecord{m_profile, m_originalBackup}
                );
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
            if (!movedIncompleteProfile)
            {
                result.incompleteRecovery = {};
            }
        }

        if (
            !movedIncompleteProfile
            || !m_port.moveProfile(m_originalBackup, m_profile)
            )
        {
            if (movedIncompleteProfile && !result.incompleteRecovery.empty())
            {
                if (
                    !m_port.profileExists(result.incompleteRecovery)
                    || m_port.moveProfile(result.incompleteRecovery, m_profile)
                    || !m_port.profileExists(result.incompleteRecovery)
                    )
                {
                    result.incompleteRecovery = {};
                }
            }

            result.status =
                InitialSetupCancelStatus::OriginalProfileRestoreFailed;
            clearPendingSetup();
            return result;
        }

        m_port.clearPendingRecovery(
            InitialSetupRecoveryRecord{m_profile, m_originalBackup}
            );

        if (
            !result.incompleteRecovery.empty()
            && m_port.profileExists(result.incompleteRecovery)
            )
        {
            // The original profile is already restored. A failed cleanup only
            // leaves an extra recoverable setup artifact.
            if (
                !m_port.removeProfile(result.incompleteRecovery)
                && m_port.profileExists(result.incompleteRecovery)
                )
            {
                result.status =
                    InitialSetupCancelStatus::
                        OriginalRestoredRecoveryRemovalFailed;
                clearPendingSetup();
                return result;
            }
        }

        result.incompleteRecovery = {};
        result.status = InitialSetupCancelStatus::Canceled;
        clearPendingSetup();
        return result;
    }

    [[nodiscard]] bool hasPendingSetup() const noexcept
    {
        return m_hasPendingSetup;
    }

    [[nodiscard]] const WorkspaceLocation& pendingProfile() const noexcept
    {
        return m_profile;
    }

    [[nodiscard]] const WorkspaceLocation& pendingOriginalBackup() const noexcept
    {
        return m_originalBackup;
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
