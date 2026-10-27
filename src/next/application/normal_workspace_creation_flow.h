#pragma once

#include "next/application/file_dialog_port.h"
#include "next/application/notification_port.h"
#include "next/application/recent_workspace_history_use_case.h"
#include "next/application/user_prompt_port.h"
#include "next/application/workspace_creation_lifecycle.h"
#include "next/application/workspace_coordinator.h"
#include "next/application/workspace_state.h"

#include <functional>
#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

// Native path details stay at the platform edge. This policy normalizes the
// accepted dialog path and compares locations using host filesystem rules.
class WorkspaceProfilePathPolicyPort
{
public:
    virtual ~WorkspaceProfilePathPolicyPort() = default;

    [[nodiscard]] virtual Domain::Result<WorkspaceLocation>
    normalizeNewProfilePath(const FileDialogPath& path) const = 0;

    [[nodiscard]] virtual bool sameLocation(
        const WorkspaceLocation& left,
        const WorkspaceLocation& right
        ) const = 0;
};

enum class NormalWorkspaceCreationStatus
{
    Created,
    Canceled,
    Blocked,
    Failed
};

struct NormalWorkspaceCreationOutcome final
{
    NormalWorkspaceCreationStatus status = NormalWorkspaceCreationStatus::Failed;
    WorkspaceLocation profile;
    std::string message;
};

// Orchestrates only the normal New Teacher Profile action. Initial Setup has
// its own lifecycle and is never started here.
class NormalWorkspaceCreationFlow final
{
public:
    using SetupPendingCheck = std::function<bool()>;

    NormalWorkspaceCreationFlow(
        FileDialogPort& fileDialog,
        UserPromptPort& userPrompt,
        WorkspaceProfilePathPolicyPort& pathPolicy,
        WorkspaceCreationLifecyclePort& lifecyclePort,
        WorkspaceCreationLifecycle& lifecycle,
        WorkspaceCoordinator& workspaceCoordinator,
        WorkspaceState& workspaceState,
        RecentWorkspaceHistoryPort& recentHistory,
        NotificationPort& notifications,
        SetupPendingCheck setupPending
        ) noexcept
        : m_fileDialog(fileDialog),
          m_userPrompt(userPrompt),
          m_pathPolicy(pathPolicy),
          m_lifecyclePort(lifecyclePort),
          m_lifecycle(lifecycle),
          m_workspaceCoordinator(workspaceCoordinator),
          m_workspaceState(workspaceState),
          m_recentHistory(recentHistory),
          m_notifications(notifications),
          m_setupPending(std::move(setupPending))
    {
    }

    [[nodiscard]] NormalWorkspaceCreationOutcome run()
    {
        if (m_setupPending && m_setupPending())
        {
            return fail(
                NormalWorkspaceCreationStatus::Blocked,
                "A profile setup or recovery is already in progress. Finish or resolve it before creating another profile.",
                NotificationSeverity::Warning
                );
        }

        const Domain::Result<ConfirmationResponse> confirmation =
            m_userPrompt.confirm(ConfirmationPromptRequest{
                .title = "New Teacher Profile",
                .message = "Leave the current page and create a new Teacher Profile?",
                .severity = PromptSeverity::Information,
                .acceptText = "Continue",
                .declineText = "Cancel",
                .defaultAction = ConfirmationAction::Decline,
                .escapeAction = ConfirmationAction::Decline
            });
        if (!confirmation)
        {
            return fail(
                NormalWorkspaceCreationStatus::Failed,
                "The current page could not confirm leaving: "
                    + confirmation.error().message
                );
        }
        if (confirmation.value() != ConfirmationResponse::Accepted)
        {
            return {NormalWorkspaceCreationStatus::Canceled, {}, {}};
        }

        const Domain::Result<
            FileDialogResponse<SavedFileDialogSelection>> selected =
            m_fileDialog.saveFile(SaveFileDialogRequest{
                .purpose = FileDialogPurpose::TeacherProfile,
                .title = "New Teacher Profile",
                .suggestedFileName = "ClassMngr.tps",
                .nameFilters = {"ClassMngr Teacher Profile (*.tps)"},
                .defaultSuffix = "tps",
                .confirmOverwrite = true
            });
        if (!selected)
        {
            return fail(
                NormalWorkspaceCreationStatus::Failed,
                "The profile save dialog could not be opened: "
                    + selected.error().message
                );
        }
        if (!selected.value().selection.has_value())
        {
            return {NormalWorkspaceCreationStatus::Canceled, {}, {}};
        }

        std::string warnings;
        if (selected.value().preferenceWarning.has_value())
        {
            append(warnings,
                   "The profile directory could not be remembered: "
                       + selected.value().preferenceWarning->message);
        }

        const Domain::Result<WorkspaceLocation> normalized =
            m_pathPolicy.normalizeNewProfilePath(
                selected.value().selection->path
                );
        if (!normalized)
        {
            return fail(
                NormalWorkspaceCreationStatus::Failed,
                "The profile destination is invalid: "
                    + normalized.error().message
                );
        }

        const auto beforeClose = m_workspaceState.snapshot();
        const std::optional<WorkspaceSession> previousSession =
            beforeClose.session();
        const Domain::Result<void> closed =
            m_lifecyclePort.closeActiveWorkspace();
        if (!closed)
        {
            return fail(
                NormalWorkspaceCreationStatus::Failed,
                "The active profile could not be closed: " + closed.error().message
                );
        }

        const WorkspaceCreationLifecycleResult created =
            m_lifecycle.createReplacingProfile(normalized.value());
        if (!created.created)
        {
            std::string message = created.error.message.empty()
                ? "The new Teacher Profile could not be created."
                : created.error.message;
            const bool sameAsPrevious = previousSession.has_value()
                && m_pathPolicy.sameLocation(
                    previousSession->location(),
                    normalized.value()
                    );
            const bool mayReopenPrevious = previousSession.has_value()
                && created.workspaceClosed
                && (!sameAsPrevious
                    || (created.originalProfileRestored
                        && !created.partialReplacementRemainsAtTarget));

            if (mayReopenPrevious)
            {
                const Domain::Result<WorkspaceSession> reopened =
                    m_workspaceCoordinator.openWorkspace(
                        OpenWorkspaceRequest{previousSession->location()}
                        );
                if (!reopened)
                {
                    append(
                        message,
                        "The previous profile could not be reopened automatically: "
                            + reopened.error().message
                            + " Profile path: "
                            + previousSession->location().value()
                        );
                }
            }
            else if (previousSession.has_value() && !created.workspaceClosed)
            {
                append(
                    message,
                    "The previous profile was not reopened because the failed replacement workspace could not be closed."
                    );
            }
            else if (previousSession.has_value() && sameAsPrevious)
            {
                append(
                    message,
                    "The previous profile could not be safely reopened from the selected target."
                    );
            }

            appendRecoveryDetails(message, created, normalized.value());
            append(message, warnings);
            notify(message, NotificationSeverity::Error);
            return {
                NormalWorkspaceCreationStatus::Failed,
                normalized.value(),
                std::move(message)
            };
        }

        const RecentWorkspaceHistory recorded =
            RecentWorkspaceHistoryUseCase::record(
                m_recentHistory.load(),
                normalized.value(),
                created.createdProfile
                );
        const Domain::Result<void> historySaved =
            m_recentHistory.save(recorded);
        if (!historySaved)
        {
            append(
                warnings,
                "The profile was created, but recent profile history could not be saved: "
                    + historySaved.error().message
                );
        }
        if (!created.originalRecoveryPath.empty())
        {
            append(
                warnings,
                "The profile was created, but the replaced original was retained at: "
                    + created.originalRecoveryPath.value()
                );
        }

        if (warnings.empty())
        {
            notify("New Teacher Profile created.", NotificationSeverity::Information);
        }
        else
        {
            notify(warnings, NotificationSeverity::Warning);
        }

        return {
            NormalWorkspaceCreationStatus::Created,
            created.createdProfile,
            std::move(warnings)
        };
    }

private:
    [[nodiscard]] NormalWorkspaceCreationOutcome fail(
        const NormalWorkspaceCreationStatus status,
        std::string message,
        const NotificationSeverity severity = NotificationSeverity::Error
        )
    {
        notify(message, severity);
        return {status, {}, std::move(message)};
    }

    void notify(
        const std::string& message,
        const NotificationSeverity severity
        ) noexcept
    {
        try
        {
            static_cast<void>(m_notifications.show(
                NotificationRequest{message, severity}
                ));
        }
        catch (...)
        {
        }
    }

    static void append(std::string& target, std::string addition)
    {
        if (addition.empty())
        {
            return;
        }
        if (!target.empty())
        {
            target += "\n\n";
        }
        target += std::move(addition);
    }

    static void appendRecoveryDetails(
        std::string& message,
        const WorkspaceCreationLifecycleResult& result,
        const WorkspaceLocation& requestedProfile
        )
    {
        if (!result.originalRecoveryPath.empty())
        {
            append(
                message,
                "The original profile remains in its recovery copy at: "
                    + result.originalRecoveryPath.value()
                );
        }
        if (!result.partialRecoveryPath.empty())
        {
            append(
                message,
                "The incomplete profile was retained at: "
                    + result.partialRecoveryPath.value()
                );
        }
        if (result.partialReplacementRemainsAtTarget)
        {
            append(
                message,
                "An incomplete replacement may remain at: "
                    + requestedProfile.value()
                );
        }
    }

    FileDialogPort& m_fileDialog;
    UserPromptPort& m_userPrompt;
    WorkspaceProfilePathPolicyPort& m_pathPolicy;
    WorkspaceCreationLifecyclePort& m_lifecyclePort;
    WorkspaceCreationLifecycle& m_lifecycle;
    WorkspaceCoordinator& m_workspaceCoordinator;
    WorkspaceState& m_workspaceState;
    RecentWorkspaceHistoryPort& m_recentHistory;
    NotificationPort& m_notifications;
    SetupPendingCheck m_setupPending;
};

} // namespace ClassMngr::Next::Application
