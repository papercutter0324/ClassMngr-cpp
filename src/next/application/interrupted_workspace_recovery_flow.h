#pragma once

#include "next/application/file_dialog_port.h"
#include "next/application/file_dialog_directory_preferences.h"
#include "next/application/notification_port.h"
#include "next/application/normal_workspace_creation_flow.h"
#include "next/application/open_existing_workspace_flow.h"
#include "next/application/page_changes_port.h"
#include "next/application/unsaved_changes_resolution_flow.h"
#include "next/application/user_prompt_port.h"
#include "next/application/workspace_coordinator.h"
#include "next/application/workspace_open_profile_path_policy.h"
#include "next/application/workspace_state.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

struct InterruptedWorkspaceRecoveryCandidate final
{
    FileDialogPath sourcePath;
    FileDialogPath targetPath;
    std::string targetFileName;
};

// Filesystem inspection and copy semantics stay at the platform boundary.
// This port is intentionally separate from Initial Setup's durable journal.
class InterruptedWorkspaceRecoveryPort
{
public:
    virtual ~InterruptedWorkspaceRecoveryPort() = default;

    [[nodiscard]] virtual FileDialogPath defaultTeacherProfileDirectory()
        const = 0;
    [[nodiscard]] virtual FileDialogPath profileDirectoryFor(
        const WorkspaceLocation& profile
        ) const = 0;
    [[nodiscard]] virtual Domain::Result<
        std::vector<InterruptedWorkspaceRecoveryCandidate>>
    candidatesIn(const FileDialogPath& directory) const = 0;
    [[nodiscard]] virtual bool pathExists(const FileDialogPath& path) const = 0;
    [[nodiscard]] virtual Domain::Result<void> copyCandidateNoReplace(
        const InterruptedWorkspaceRecoveryCandidate& candidate,
        const WorkspaceLocation& destination
        ) const = 0;
};

enum class InterruptedWorkspaceRecoveryStatus
{
    Recovered,
    NoCandidates,
    Canceled,
    Blocked,
    Failed
};

struct InterruptedWorkspaceRecoveryOutcome final
{
    InterruptedWorkspaceRecoveryStatus status =
        InterruptedWorkspaceRecoveryStatus::Failed;
    std::optional<WorkspaceSession> session;
    WorkspaceLocation destination;
    std::string message;
};

// Explicitly copies one interrupted normal-create recovery sibling to a new
// profile, then uses the normal open flow to commit the active session/history.
class InterruptedWorkspaceRecoveryFlow final
{
public:
    using SetupPendingCheck = std::function<bool()>;

    InterruptedWorkspaceRecoveryFlow(
        FileDialogPort& fileDialog,
        UserPromptPort& userPrompt,
        FileDialogDirectoryPreferencesPort& directoryPreferences,
        InterruptedWorkspaceRecoveryPort& recoveryPort,
        WorkspaceProfilePathPolicyPort& pathPolicy,
        ActivePageChangesPort& activePageChanges,
        WorkspaceCoordinator& workspaceCoordinator,
        WorkspaceState& workspaceState,
        OpenExistingWorkspaceFlow& openExistingFlow,
        NotificationPort& notifications,
        SetupPendingCheck setupPending
        ) noexcept
        : m_fileDialog(fileDialog),
          m_userPrompt(userPrompt),
          m_directoryPreferences(directoryPreferences),
          m_recoveryPort(recoveryPort),
          m_pathPolicy(pathPolicy),
          m_activePageChanges(activePageChanges),
          m_workspaceCoordinator(workspaceCoordinator),
          m_workspaceState(workspaceState),
          m_openExistingFlow(openExistingFlow),
          m_notifications(notifications),
          m_setupPending(std::move(setupPending))
    {
    }

    [[nodiscard]] InterruptedWorkspaceRecoveryOutcome run()
    {
        if (m_setupPending && m_setupPending())
        {
            return fail(
                InterruptedWorkspaceRecoveryStatus::Blocked,
                "A profile setup or recovery is already in progress. Finish or resolve it before recovering another profile.",
                NotificationSeverity::Warning
                );
        }

        const WorkspaceStateSnapshot snapshot = m_workspaceState.snapshot();
        FileDialogPath scanDirectory;
        if (snapshot.session().has_value())
        {
            scanDirectory = m_recoveryPort.profileDirectoryFor(
                snapshot.session()->location()
                );
        }
        else
        {
            const DirectoryPreferencePath remembered =
                m_directoryPreferences.readDirectory(
                    FileDialogPurpose::TeacherProfile
                    );
            scanDirectory = remembered.empty()
                ? m_recoveryPort.defaultTeacherProfileDirectory()
                : FileDialogPath(remembered.value());
        }

        const Domain::Result<
            std::vector<InterruptedWorkspaceRecoveryCandidate>> candidates =
            m_recoveryPort.candidatesIn(scanDirectory);
        if (!candidates)
        {
            return fail(
                InterruptedWorkspaceRecoveryStatus::Failed,
                "Interrupted profile recovery files could not be scanned in "
                    + scanDirectory.value() + ": " + candidates.error().message
                );
        }
        if (candidates.value().empty())
        {
            const std::string message =
                "No interrupted profile recovery copies were found in:\n"
                + scanDirectory.value();
            const Domain::Result<void> shown = m_userPrompt.showMessage(
                MessagePromptRequest{
                    .title = "Recover Interrupted Teacher Profile",
                    .message = message,
                    .severity = PromptSeverity::Information,
                    .acknowledgeText = "OK"
                }
                );
            if (!shown)
            {
                return fail(
                    InterruptedWorkspaceRecoveryStatus::Failed,
                    "The no-candidate recovery result could not be shown: "
                        + shown.error().message
                    );
            }
            return {
                InterruptedWorkspaceRecoveryStatus::NoCandidates,
                std::nullopt,
                {},
                message
            };
        }

        const Domain::Result<std::optional<std::size_t>> selectedIndex =
            selectCandidate(scanDirectory, candidates.value());
        if (!selectedIndex)
        {
            return fail(
                InterruptedWorkspaceRecoveryStatus::Failed,
                selectedIndex.error().message
                );
        }
        if (!selectedIndex.value().has_value())
        {
            return {InterruptedWorkspaceRecoveryStatus::Canceled};
        }
        const InterruptedWorkspaceRecoveryCandidate& candidate =
            candidates.value()[*selectedIndex.value()];

        const std::string suggestedName = m_recoveryPort.pathExists(
                candidate.targetPath
                )
            ? "Recovered " + candidate.targetFileName
            : candidate.targetFileName;

        while (true)
        {
            const auto destination = chooseUnusedDestination(
                scanDirectory,
                suggestedName
                );
            if (destination.status == DestinationChoiceStatus::Canceled)
            {
                return {InterruptedWorkspaceRecoveryStatus::Canceled};
            }
            if (destination.status == DestinationChoiceStatus::Failed)
            {
                return fail(
                    InterruptedWorkspaceRecoveryStatus::Failed,
                    destination.message
                    );
            }

            const UnsavedChangesResolution resolution =
                resolveUnsavedChanges();
            if (resolution.status == UnsavedChangesResolutionStatus::Canceled)
            {
                return {InterruptedWorkspaceRecoveryStatus::Canceled};
            }
            if (resolution.status == UnsavedChangesResolutionStatus::Failed)
            {
                return fail(
                    InterruptedWorkspaceRecoveryStatus::Failed,
                    resolution.message
                    );
            }

            const Domain::Result<void> copied =
                m_recoveryPort.copyCandidateNoReplace(
                    candidate,
                    destination.location
                    );
            if (!copied
                && copied.error().code == Domain::ErrorCode::Conflict)
            {
                warn(
                    "The destination became occupied and was left unchanged. Choose a different path:\n"
                    + destination.location.value()
                    );
                continue;
            }
            if (!copied)
            {
                return fail(
                    InterruptedWorkspaceRecoveryStatus::Failed,
                    "The recovery copy could not be copied. The source remains at:\n"
                        + candidate.sourcePath.value()
                        + "\n\nThe destination, if created, remains at:\n"
                        + destination.location.value() + "\n\n"
                        + copied.error().message
                    );
            }

            const OpenExistingWorkspaceOutcome opened =
                m_openExistingFlow.openPath(
                    FileDialogPath(destination.location.value()),
                    OpenExistingWorkspaceMissingPathPolicy::PreserveHistory
                    );
            if (opened.status != OpenExistingWorkspaceStatus::Opened
                || !opened.session.has_value())
            {
                const std::string reason = opened.message.empty()
                    ? "The copied profile could not be opened."
                    : opened.message;
                return fail(
                    InterruptedWorkspaceRecoveryStatus::Failed,
                    "The copied profile could not be opened. The copy remains at:\n"
                        + destination.location.value()
                        + "\n\nThe original recovery file remains at:\n"
                        + candidate.sourcePath.value() + "\n\n" + reason
                    );
            }

            return {
                InterruptedWorkspaceRecoveryStatus::Recovered,
                opened.session,
                destination.location,
                "Interrupted Teacher Profile recovered."
            };
        }
    }

private:
    enum class DestinationChoiceStatus
    {
        Selected,
        Canceled,
        Failed
    };

    struct DestinationChoice final
    {
        DestinationChoiceStatus status = DestinationChoiceStatus::Failed;
        WorkspaceLocation location;
        std::string message;
    };

    [[nodiscard]] Domain::Result<std::optional<std::size_t>> selectCandidate(
        const FileDialogPath& directory,
        const std::vector<InterruptedWorkspaceRecoveryCandidate>& candidates
        )
    {
        NamedActionPromptRequest request{
            .title = "Recover Interrupted Teacher Profile",
            .message = "Choose a recovery copy to copy into a Teacher Profile file. The recovery copy will remain unchanged.",
            .informativeText = "Candidates were found in:\n" + directory.value()
                + "\n\nTarget and recovery-copy paths:",
            .severity = PromptSeverity::Information
        };
        request.actions.reserve(candidates.size() + 1);
        for (std::size_t index = 0; index < candidates.size(); ++index)
        {
            const InterruptedWorkspaceRecoveryCandidate& candidate =
                candidates[index];
            const std::string id = "candidate-" + std::to_string(index);
            const std::optional<PromptActionId> actionId =
                PromptActionId::create(id);
            if (!actionId.has_value())
            {
                return Domain::Result<std::optional<std::size_t>>::failure({
                    .code = Domain::ErrorCode::InvalidInput,
                    .message = "Too many recovery candidates were found to display safely.",
                    .recoverable = false
                });
            }
            request.informativeText += "\n\n" + candidate.targetFileName
                + "\n" + candidate.targetPath.value() + "\n"
                + candidate.sourcePath.value();
            request.actions.push_back(PromptAction{
                .id = *actionId,
                .text = candidate.targetFileName + " — "
                    + candidate.sourcePath.value(),
                .role = PromptActionRole::Action,
                .enabled = true
            });
        }
        const PromptActionId cancelId = *PromptActionId::create("cancel");
        request.actions.push_back(PromptAction{
            .id = cancelId,
            .text = "Cancel",
            .role = PromptActionRole::Reject,
            .enabled = true
        });
        request.defaultActionId = cancelId;
        request.escapeActionId = cancelId;

        const Domain::Result<std::optional<PromptActionId>> selected =
            m_userPrompt.chooseAction(request);
        if (!selected)
        {
            return Domain::Result<std::optional<std::size_t>>::failure({
                .code = selected.error().code,
                .message = "The recovery candidate prompt failed: "
                    + selected.error().message,
                .recoverable = selected.error().recoverable
            });
        }
        if (!selected.value().has_value()
            || selected.value().value() == cancelId)
        {
            return Domain::Result<std::optional<std::size_t>>::success(
                std::nullopt
                );
        }
        const std::string& selectedId = selected.value()->value();
        for (std::size_t index = 0; index < candidates.size(); ++index)
        {
            if (selectedId == "candidate-" + std::to_string(index))
            {
                return Domain::Result<std::optional<std::size_t>>::success(
                    std::optional<std::size_t>(index)
                    );
            }
        }
        return Domain::Result<std::optional<std::size_t>>::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message = "The recovery candidate selection was invalid.",
            .recoverable = false
        });
    }

    [[nodiscard]] DestinationChoice chooseUnusedDestination(
        const FileDialogPath& initialDirectory,
        const std::string& suggestedName
        )
    {
        while (true)
        {
            const Domain::Result<
                FileDialogResponse<SavedFileDialogSelection>> selected =
                m_fileDialog.saveFile(SaveFileDialogRequest{
                    .purpose = FileDialogPurpose::TeacherProfile,
                    .title = "Save Recovered Teacher Profile",
                    .initialDirectory = initialDirectory,
                    .suggestedFileName = suggestedName,
                    .nameFilters = {"ClassMngr Teacher Profile (*.tps)"},
                    .defaultSuffix = "tps",
                    .confirmOverwrite = false
                });
            if (!selected)
            {
                return {
                    DestinationChoiceStatus::Failed,
                    {},
                    "The recovered profile save dialog could not be opened: "
                        + selected.error().message
                };
            }
            if (!selected.value().selection.has_value())
            {
                return {DestinationChoiceStatus::Canceled};
            }

            const Domain::Result<WorkspaceLocation> normalized =
                m_pathPolicy.normalizeNewProfilePath(
                    selected.value().selection->path
                    );
            if (!normalized)
            {
                warn("The profile destination is invalid: "
                     + normalized.error().message);
                continue;
            }
            const FileDialogPath normalizedPath(normalized.value().value());
            if (m_recoveryPort.pathExists(normalizedPath))
            {
                warn(
                    "The destination already exists and was left unchanged. Choose a different path:\n"
                    + normalized.value().value()
                    );
                continue;
            }
            return {
                DestinationChoiceStatus::Selected,
                normalized.value(),
                {}
            };
        }
    }

    [[nodiscard]] UnsavedChangesResolution resolveUnsavedChanges()
    {
        UnsavedChangesResolutionFlow flow(
            m_activePageChanges,
            m_workspaceCoordinator,
            m_userPrompt
            );
        return flow.resolve(UnsavedChangesPromptRequest{
            .title = "Unsaved Changes",
            .message = "The current page or profile has unsaved changes.",
            .question = "Save changes before recovering another Teacher Profile?",
            .saveText = "Save",
            .discardText = "Discard",
            .cancelText = "Cancel"
        });
    }

    [[nodiscard]] InterruptedWorkspaceRecoveryOutcome fail(
        const InterruptedWorkspaceRecoveryStatus status,
        std::string message,
        const NotificationSeverity severity = NotificationSeverity::Error
        )
    {
        notify(message, severity);
        return {status, std::nullopt, {}, std::move(message)};
    }

    void warn(const std::string& message)
    {
        notify(message, NotificationSeverity::Warning);
    }

    void notify(
        const std::string& message,
        const NotificationSeverity severity
        )
    {
        static_cast<void>(m_notifications.show(NotificationRequest{
            .message = message,
            .severity = severity
        }));
    }

    FileDialogPort& m_fileDialog;
    UserPromptPort& m_userPrompt;
    FileDialogDirectoryPreferencesPort& m_directoryPreferences;
    InterruptedWorkspaceRecoveryPort& m_recoveryPort;
    WorkspaceProfilePathPolicyPort& m_pathPolicy;
    ActivePageChangesPort& m_activePageChanges;
    WorkspaceCoordinator& m_workspaceCoordinator;
    WorkspaceState& m_workspaceState;
    OpenExistingWorkspaceFlow& m_openExistingFlow;
    NotificationPort& m_notifications;
    SetupPendingCheck m_setupPending;
};

} // namespace ClassMngr::Next::Application
