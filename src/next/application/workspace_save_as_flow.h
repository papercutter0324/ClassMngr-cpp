#pragma once

#include "next/application/file_dialog_port.h"
#include "next/application/normal_workspace_creation_flow.h"
#include "next/application/notification_port.h"
#include "next/application/recent_workspace_history_use_case.h"
#include "next/application/workspace_coordinator.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

enum class WorkspaceSaveAsStatus
{
    Saved,
    Canceled,
    Blocked,
    Failed
};

struct WorkspaceSaveAsOutcome final
{
    WorkspaceSaveAsStatus status = WorkspaceSaveAsStatus::Failed;
    std::optional<WorkspaceLocation> location;
    std::string message;
};

// Owns the interactive Save As sequence. The gateway copies and reopens the
// profile before the coordinator changes the active session location.
class WorkspaceSaveAsFlow final
{
public:
    using SetupPendingCheck = std::function<bool()>;

    WorkspaceSaveAsFlow(
        FileDialogPort& fileDialog,
        WorkspaceProfilePathPolicyPort& pathPolicy,
        WorkspaceCoordinator& workspaceCoordinator,
        RecentWorkspaceHistoryPort* recentHistory,
        NotificationPort& notifications,
        FileDialogPath initialDirectory,
        std::string suggestedFileName,
        SetupPendingCheck setupPending
        ) noexcept
        : m_fileDialog(fileDialog),
          m_pathPolicy(pathPolicy),
          m_workspaceCoordinator(workspaceCoordinator),
          m_recentHistory(recentHistory),
          m_notifications(notifications),
          m_initialDirectory(std::move(initialDirectory)),
          m_suggestedFileName(std::move(suggestedFileName)),
          m_setupPending(std::move(setupPending))
    {
    }

    [[nodiscard]] WorkspaceSaveAsOutcome run()
    {
        if (m_setupPending && m_setupPending())
        {
            return fail(
                WorkspaceSaveAsStatus::Blocked,
                "Resolve the pending profile setup or recovery state before using Save As.",
                NotificationSeverity::Warning
                );
        }

        const Domain::Result<FileDialogResponse<SavedFileDialogSelection>>
            selected = m_fileDialog.saveFile(SaveFileDialogRequest{
                .purpose = FileDialogPurpose::TeacherProfile,
                .title = "Save Teacher Profile",
                .initialDirectory = m_initialDirectory,
                .suggestedFileName = m_suggestedFileName,
                .nameFilters = {"ClassMngr Teacher Profile (*.tps)"},
                .defaultSuffix = "tps",
                .confirmOverwrite = true
            });
        if (!selected)
        {
            return fail(
                WorkspaceSaveAsStatus::Failed,
                "The profile save dialog could not be opened: "
                    + selected.error().message
                );
        }
        if (!selected.value().selection.has_value())
        {
            return {WorkspaceSaveAsStatus::Canceled, std::nullopt, {}};
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

        const Domain::Result<WorkspaceLocation> normalized =
            m_pathPolicy.normalizeNewProfilePath(
                selected.value().selection->path
                );
        if (!normalized)
        {
            return fail(
                WorkspaceSaveAsStatus::Failed,
                "The profile destination is invalid: "
                    + normalized.error().message,
                NotificationSeverity::Error,
                std::move(warnings)
                );
        }

        const Domain::Result<WorkspaceLocation> saved =
            m_workspaceCoordinator.saveWorkspaceAs(normalized.value());
        if (!saved)
        {
            return fail(
                WorkspaceSaveAsStatus::Failed,
                "The Teacher Profile could not be saved as the selected file: "
                    + saved.error().message,
                NotificationSeverity::Error,
                std::move(warnings)
                );
        }

        if (m_recentHistory)
        {
            try
            {
                const RecentWorkspaceHistory updated =
                    RecentWorkspaceHistoryUseCase::record(
                        m_recentHistory->load(),
                        RecentWorkspacePath(
                            selected.value().selection->path.value()
                            ),
                        saved.value()
                        );
                const Domain::Result<void> historySaved =
                    m_recentHistory->save(updated);
                if (!historySaved)
                {
                    append(
                        warnings,
                        "The profile was saved, but recent profile history could not be saved: "
                            + historySaved.error().message
                        );
                }
            }
            catch (...)
            {
                append(
                    warnings,
                    "The profile was saved, but recent profile history could not be saved."
                    );
            }
        }
        else
        {
            append(
                warnings,
                "The profile was saved, but recent profile history is unavailable."
                );
        }

        const std::string message = warnings.empty()
            ? "Teacher Profile saved as a new file."
            : warnings;
        notify(
            message,
            warnings.empty()
                ? NotificationSeverity::Information
                : NotificationSeverity::Warning
            );
        return {
            WorkspaceSaveAsStatus::Saved,
            saved.value(),
            message
        };
    }

private:
    [[nodiscard]] WorkspaceSaveAsOutcome fail(
        const WorkspaceSaveAsStatus status,
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

    FileDialogPort& m_fileDialog;
    WorkspaceProfilePathPolicyPort& m_pathPolicy;
    WorkspaceCoordinator& m_workspaceCoordinator;
    RecentWorkspaceHistoryPort* m_recentHistory = nullptr;
    NotificationPort& m_notifications;
    FileDialogPath m_initialDirectory;
    std::string m_suggestedFileName;
    SetupPendingCheck m_setupPending;
};

} // namespace ClassMngr::Next::Application
