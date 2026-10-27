#pragma once

#include "next/application/file_dialog_port.h"
#include "next/application/notification_port.h"
#include "next/application/recent_workspace_history_use_case.h"
#include "next/application/workspace_coordinator.h"
#include "next/application/workspace_open_profile_path_policy.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

enum class OpenExistingWorkspaceStatus
{
    Opened,
    Canceled,
    Blocked,
    Failed
};

enum class OpenExistingWorkspaceMissingPathPolicy
{
    PruneHistory,
    PreserveHistory
};

struct OpenExistingWorkspaceOutcome final
{
    OpenExistingWorkspaceStatus status = OpenExistingWorkspaceStatus::Failed;
    std::optional<WorkspaceSession> session;
    std::string message;
};

// Owns only the interactive open-existing-profile sequence. It never closes
// the active store; WorkspaceCoordinator commits a new session only after the
// gateway successfully opens and validates the selected profile.
class OpenExistingWorkspaceFlow final
{
public:
    using SetupPendingCheck = std::function<bool()>;

    OpenExistingWorkspaceFlow(
        FileDialogPort& fileDialog,
        WorkspaceOpenProfilePathPolicyPort& pathPolicy,
        WorkspaceCoordinator& workspaceCoordinator,
        RecentWorkspaceHistoryPort& recentHistory,
        NotificationPort& notifications,
        SetupPendingCheck setupPending
        ) noexcept
        : m_fileDialog(&fileDialog),
          m_pathPolicy(pathPolicy),
          m_workspaceCoordinator(workspaceCoordinator),
          m_recentHistory(recentHistory),
          m_notifications(notifications),
          m_setupPending(std::move(setupPending))
    {
    }

    // A selected recent path uses the same normalization, open, and history
    // policy as the dialog flow without constructing or showing a dialog.
    OpenExistingWorkspaceFlow(
        WorkspaceOpenProfilePathPolicyPort& pathPolicy,
        WorkspaceCoordinator& workspaceCoordinator,
        RecentWorkspaceHistoryPort& recentHistory,
        NotificationPort& notifications,
        SetupPendingCheck setupPending
        ) noexcept
        : m_pathPolicy(pathPolicy),
          m_workspaceCoordinator(workspaceCoordinator),
          m_recentHistory(recentHistory),
          m_notifications(notifications),
          m_setupPending(std::move(setupPending))
    {
    }

    [[nodiscard]] OpenExistingWorkspaceOutcome run()
    {
        if (blockedBySetupPending())
        {
            return blockedOutcome();
        }

        if (!m_fileDialog)
        {
            return fail(
                OpenExistingWorkspaceStatus::Failed,
                "The profile open dialog is unavailable."
                );
        }

        const Domain::Result<FileDialogResponse<FileDialogPath>> selected =
            m_fileDialog->openOneFile(OpenFileDialogRequest{
                .purpose = FileDialogPurpose::TeacherProfile,
                .title = "Open Teacher Profile",
                .nameFilters = {"ClassMngr Teacher Profile (*.tps *.db)"}
            });
        if (!selected)
        {
            return fail(
                OpenExistingWorkspaceStatus::Failed,
                "The profile open dialog could not be opened: "
                    + selected.error().message
                );
        }
        if (!selected.value().selection.has_value())
        {
            return {OpenExistingWorkspaceStatus::Canceled, std::nullopt, {}};
        }

        std::string warnings;
        if (selected.value().preferenceWarning.has_value())
        {
            append(
                warnings,
                "The profile directory could not be remembered: "
                    + selected.value().preferenceWarning->message
                );
        }

        return openPathAfterSetup(
            *selected.value().selection,
            std::move(warnings)
            );
    }

    [[nodiscard]] OpenExistingWorkspaceOutcome openPath(
        const FileDialogPath& path,
        const OpenExistingWorkspaceMissingPathPolicy missingPathPolicy =
            OpenExistingWorkspaceMissingPathPolicy::PruneHistory
        )
    {
        if (blockedBySetupPending())
        {
            return blockedOutcome();
        }
        return openPathAfterSetup(path, {}, missingPathPolicy);
    }

private:
    [[nodiscard]] OpenExistingWorkspaceOutcome openPathAfterSetup(
        const FileDialogPath& path,
        std::string warnings,
        const OpenExistingWorkspaceMissingPathPolicy missingPathPolicy =
            OpenExistingWorkspaceMissingPathPolicy::PruneHistory
        )
    {
        const Domain::Result<WorkspaceLocation> normalized =
            m_pathPolicy.normalizeOpenProfilePath(path);
        if (!normalized)
        {
            return fail(
                OpenExistingWorkspaceStatus::Failed,
                "The selected profile is invalid: "
                    + normalized.error().message,
                NotificationSeverity::Error,
                std::move(warnings)
                );
        }

        const Domain::Result<WorkspaceSession> opened =
            m_workspaceCoordinator.openWorkspace(
                OpenWorkspaceRequest{normalized.value()}
                );
        if (!opened)
        {
            if (opened.error().code == Domain::ErrorCode::NotFound)
            {
                if (missingPathPolicy
                    == OpenExistingWorkspaceMissingPathPolicy::PreserveHistory)
                {
                    return fail(
                        OpenExistingWorkspaceStatus::Failed,
                        "The selected Teacher Profile could not be found: "
                            + opened.error().message,
                        NotificationSeverity::Warning,
                        std::move(warnings)
                        );
                }
                const RecentWorkspaceHistory pruned =
                    RecentWorkspaceHistoryUseCase::prune(
                        m_recentHistory.load(),
                        RecentWorkspacePath(path.value()),
                        RecentWorkspacePath(normalized.value().value())
                        );
                std::string message =
                    "The selected Teacher Profile could not be found and was removed from Recent Files: "
                    + opened.error().message;
                const Domain::Result<void> historySaved =
                    m_recentHistory.save(pruned);
                if (!historySaved)
                {
                    append(
                        message,
                        "Recent profile history could not be saved: "
                            + historySaved.error().message
                        );
                }
                append(message, std::move(warnings));
                notify(message, NotificationSeverity::Warning);
                return {
                    OpenExistingWorkspaceStatus::Failed,
                    std::nullopt,
                    std::move(message)
                };
            }

            return fail(
                OpenExistingWorkspaceStatus::Failed,
                "The selected Teacher Profile could not be opened: "
                    + opened.error().message,
                NotificationSeverity::Error,
                std::move(warnings)
                );
        }

        const RecentWorkspaceHistory updated = RecentWorkspaceHistoryUseCase::record(
            m_recentHistory.load(),
            RecentWorkspacePath(path.value()),
            opened.value().location()
            );
        const Domain::Result<void> historySaved = m_recentHistory.save(updated);
        if (!historySaved)
        {
            append(
                warnings,
                "The profile was opened, but recent profile history could not be saved: "
                    + historySaved.error().message
                );
        }

        if (warnings.empty())
        {
            notify("Teacher Profile opened.", NotificationSeverity::Information);
        }
        else
        {
            notify(warnings, NotificationSeverity::Warning);
        }

        return {
            OpenExistingWorkspaceStatus::Opened,
            opened.value(),
            std::move(warnings)
        };
    }

    [[nodiscard]] bool blockedBySetupPending() const
    {
        return m_setupPending && m_setupPending();
    }

    [[nodiscard]] OpenExistingWorkspaceOutcome blockedOutcome()
    {
        return fail(
            OpenExistingWorkspaceStatus::Blocked,
            "A profile setup or recovery is in progress. Finish or resolve it before opening another profile.",
            NotificationSeverity::Warning
            );
    }

    [[nodiscard]] OpenExistingWorkspaceOutcome fail(
        const OpenExistingWorkspaceStatus status,
        std::string message,
        const NotificationSeverity severity = NotificationSeverity::Error,
        std::string warnings = {}
        )
    {
        append(message, std::move(warnings));
        notify(message, severity);
        return {status, std::nullopt, std::move(message)};
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

    FileDialogPort* m_fileDialog = nullptr;
    WorkspaceOpenProfilePathPolicyPort& m_pathPolicy;
    WorkspaceCoordinator& m_workspaceCoordinator;
    RecentWorkspaceHistoryPort& m_recentHistory;
    NotificationPort& m_notifications;
    SetupPendingCheck m_setupPending;
};

} // namespace ClassMngr::Next::Application
