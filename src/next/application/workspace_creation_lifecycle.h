#pragma once

#include "next/application/workspace_contracts.h"

#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

// Platform boundary for creating a workspace at a user-selected profile
// path. Existing profiles are moved aside until creation succeeds so a failed
// initialization can restore the selected file without losing its contents.
class WorkspaceCreationLifecyclePort
{
public:
    virtual ~WorkspaceCreationLifecyclePort() = default;

    [[nodiscard]] virtual bool profileExists(
        const WorkspaceLocation& location
        ) const = 0;

    [[nodiscard]] virtual WorkspaceLocation recoveryLocationFor(
        const WorkspaceLocation& profile
        ) = 0;

    // A failed move must leave the source intact. The destination must not be
    // replaced when it already exists.
    [[nodiscard]] virtual bool moveProfile(
        const WorkspaceLocation& source,
        const WorkspaceLocation& destination
        ) = 0;

    [[nodiscard]] virtual bool removeProfile(
        const WorkspaceLocation& location
        ) = 0;

    [[nodiscard]] virtual Domain::Result<void> closeActiveWorkspace() = 0;

    [[nodiscard]] virtual Domain::Result<WorkspaceSession> createWorkspace(
        const CreateWorkspaceRequest& request
        ) = 0;
};

struct WorkspaceCreationLifecycleResult final
{
    bool created = false;
    bool workspaceClosed = true;
    bool originalProfileExisted = false;
    bool originalProfileRestored = false;
    bool partialReplacementRemainsAtTarget = false;
    WorkspaceLocation createdProfile;
    WorkspaceLocation originalRecoveryPath;
    WorkspaceLocation partialRecoveryPath;
    Domain::OperationError error;
};

// Synchronous replacement policy for the normal New Teacher Profile flow.
// Initial Setup keeps its distinct begin/finish/cancel lifecycle.
class WorkspaceCreationLifecycle final
{
public:
    explicit WorkspaceCreationLifecycle(
        WorkspaceCreationLifecyclePort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] WorkspaceCreationLifecycleResult createReplacingProfile(
        WorkspaceLocation profile
        )
    {
        WorkspaceCreationLifecycleResult result;
        result.originalProfileExisted = m_port.profileExists(profile);
        result.originalProfileRestored = !result.originalProfileExisted;

        WorkspaceLocation originalBackup;
        if (result.originalProfileExisted)
        {
            originalBackup = uniqueRecoveryLocationFor(profile);
            if (
                originalBackup.empty()
                || m_port.profileExists(originalBackup)
                || !m_port.moveProfile(profile, originalBackup)
                )
            {
                result.originalProfileRestored = true;
                result.error = technicalFailure(
                    "The existing Teacher Profile could not be preserved."
                    );
                return result;
            }
        }

        const auto created = m_port.createWorkspace(
            CreateWorkspaceRequest{profile}
            );
        if (created)
        {
            result.created = true;
            result.createdProfile = created.value().location();
            if (
                !originalBackup.empty()
                && !m_port.removeProfile(originalBackup)
                )
            {
                // The new profile is active. Keep the old file recoverable if
                // cleanup fails and let the caller disclose its location.
                result.originalRecoveryPath = originalBackup;
            }

            return result;
        }

        result.error = created.error();
        const auto closed = m_port.closeActiveWorkspace();
        if (!closed)
        {
            result.workspaceClosed = false;
            if (!originalBackup.empty())
            {
                result.originalRecoveryPath = originalBackup;
            }
            result.partialReplacementRemainsAtTarget =
                m_port.profileExists(profile);
            return result;
        }

        if (m_port.profileExists(profile))
        {
            if (!m_port.removeProfile(profile))
            {
                const WorkspaceLocation partialRecovery =
                    uniqueRecoveryLocationFor(profile);
                if (
                    !partialRecovery.empty()
                    && !m_port.profileExists(partialRecovery)
                    && m_port.moveProfile(profile, partialRecovery)
                    )
                {
                    result.partialRecoveryPath = partialRecovery;
                }
                else
                {
                    result.partialReplacementRemainsAtTarget = true;
                }
            }
        }

        if (originalBackup.empty())
        {
            result.originalProfileRestored = true;
            return result;
        }

        if (
            !result.partialReplacementRemainsAtTarget
            && !m_port.profileExists(profile)
            && m_port.moveProfile(originalBackup, profile)
            )
        {
            result.originalProfileRestored = true;
            return result;
        }

        result.originalRecoveryPath = originalBackup;
        return result;
    }

private:
    [[nodiscard]] WorkspaceLocation uniqueRecoveryLocationFor(
        const WorkspaceLocation& profile
        )
    {
        // The platform normally generates a UUID-backed sibling name. Check
        // it before using it, and bound retries if the port cannot provide a
        // free location.
        constexpr int kMaximumAttempts = 8;
        for (int attempt = 0; attempt < kMaximumAttempts; ++attempt)
        {
            WorkspaceLocation candidate =
                m_port.recoveryLocationFor(profile);
            if (!candidate.empty() && !m_port.profileExists(candidate))
            {
                return candidate;
            }
        }

        return WorkspaceLocation{};
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

    WorkspaceCreationLifecyclePort& m_port;
};

} // namespace ClassMngr::Next::Application
