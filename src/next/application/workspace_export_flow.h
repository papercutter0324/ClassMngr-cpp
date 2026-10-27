#pragma once

#include "next/application/file_dialog_port.h"
#include "next/application/normal_workspace_creation_flow.h"
#include "next/application/notification_port.h"
#include "next/application/workspace_coordinator.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

enum class WorkspaceExportStatus
{
    Exported,
    Canceled,
    Blocked,
    Failed
};

struct WorkspaceExportOutcome final
{
    WorkspaceExportStatus status = WorkspaceExportStatus::Failed;
    std::optional<WorkspaceLocation> location;
    std::string message;
};

// Owns the interactive profile-export sequence. Export copies the committed
// database snapshot without changing the active workspace or application state.
class WorkspaceExportFlow final
{
public:
    using SetupPendingCheck = std::function<bool()>;

    WorkspaceExportFlow(
        FileDialogPort& fileDialog,
        WorkspaceProfilePathPolicyPort& pathPolicy,
        WorkspaceCoordinator& workspaceCoordinator,
        NotificationPort& notifications,
        FileDialogPath initialDirectory,
        SetupPendingCheck setupPending
        ) noexcept
        : m_fileDialog(fileDialog),
          m_pathPolicy(pathPolicy),
          m_workspaceCoordinator(workspaceCoordinator),
          m_notifications(notifications),
          m_initialDirectory(std::move(initialDirectory)),
          m_setupPending(std::move(setupPending))
    {
    }

    [[nodiscard]] WorkspaceExportOutcome run()
    {
        if (m_setupPending && m_setupPending())
        {
            return fail(
                WorkspaceExportStatus::Blocked,
                "Resolve the pending profile setup or recovery state before exporting a Teacher Profile.",
                NotificationSeverity::Warning
                );
        }

        const Domain::Result<
            FileDialogResponse<SavedFileDialogSelection>> selected =
            m_fileDialog.saveFile(SaveFileDialogRequest{
                .purpose = FileDialogPurpose::ExportTeacherProfile,
                .title = "Export Teacher Profile",
                .initialDirectory = m_initialDirectory,
                .nameFilters = {"ClassMngr Teacher Profile (*.tps)"},
                .defaultSuffix = "tps",
                .confirmOverwrite = true
            });
        if (!selected)
        {
            return fail(
                WorkspaceExportStatus::Failed,
                "The profile export dialog could not be opened: "
                    + selected.error().message
                );
        }
        if (!selected.value().selection.has_value())
        {
            return {WorkspaceExportStatus::Canceled, std::nullopt, {}};
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
                WorkspaceExportStatus::Failed,
                "The profile export destination is invalid: "
                    + normalized.error().message,
                NotificationSeverity::Error,
                std::move(warnings)
                );
        }

        const Domain::Result<WorkspaceLocation> exported =
            m_workspaceCoordinator.exportWorkspace(normalized.value());
        if (!exported)
        {
            return fail(
                WorkspaceExportStatus::Failed,
                "The Teacher Profile could not be exported to the selected file: "
                    + exported.error().message,
                NotificationSeverity::Error,
                std::move(warnings)
                );
        }

        const std::string message = warnings.empty()
            ? "Teacher Profile exported."
            : warnings;
        notify(
            message,
            warnings.empty()
                ? NotificationSeverity::Information
                : NotificationSeverity::Warning
            );
        return {
            WorkspaceExportStatus::Exported,
            exported.value(),
            message
        };
    }

private:
    [[nodiscard]] WorkspaceExportOutcome fail(
        const WorkspaceExportStatus status,
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
    NotificationPort& m_notifications;
    FileDialogPath m_initialDirectory;
    SetupPendingCheck m_setupPending;
};

} // namespace ClassMngr::Next::Application
