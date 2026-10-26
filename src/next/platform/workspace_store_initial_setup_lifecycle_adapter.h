#pragma once

#include "next/application/file_dialog_port.h"
#include "next/application/initial_setup_lifecycle.h"
#include "next/application/workspace_coordinator.h"
#include "next/application/workspace_state.h"

#include <QString>

namespace ClassMngr::Next::Platform
{

// Store-backed setup boundary. It keeps native path handling, profile file
// operations, and the durable interrupted-setup journal at the Platform edge.
class WorkspaceStoreInitialSetupLifecycleAdapter final
    : public Application::InitialSetupLifecyclePort
{
public:
    WorkspaceStoreInitialSetupLifecycleAdapter(
        Application::WorkspaceCoordinator& workspaceCoordinator,
        Application::WorkspaceState& workspaceState,
        QString recoveryJournalPath = {}
        ) noexcept;

    [[nodiscard]] bool hasActiveWorkspace() const override;
    [[nodiscard]] Domain::Result<void> closeActiveWorkspace() override;
    [[nodiscard]] bool profileExists(
        const Application::WorkspaceLocation& location
        ) const override;
    [[nodiscard]] Application::WorkspaceLocation recoveryLocationFor(
        const Application::WorkspaceLocation& profile
        ) override;
    [[nodiscard]] bool moveProfile(
        const Application::WorkspaceLocation& source,
        const Application::WorkspaceLocation& destination
        ) override;
    [[nodiscard]] bool removeProfile(
        const Application::WorkspaceLocation& location
        ) override;
    [[nodiscard]] Domain::Result<Application::WorkspaceSession>
    createWorkspace(
        const Application::CreateWorkspaceRequest& request
        ) override;
    [[nodiscard]] Domain::Result<Application::WorkspaceSession>
    openWorkspace(
        const Application::OpenWorkspaceRequest& request
        ) override;
    [[nodiscard]] Domain::Result<
        std::optional<Application::InitialSetupRecoveryRecord>>
    pendingRecovery() const override;
    [[nodiscard]] bool recordPendingRecovery(
        const Application::InitialSetupRecoveryRecord& record
        ) override;
    void clearPendingRecovery(
        const Application::InitialSetupRecoveryRecord& record
        ) noexcept override;

    [[nodiscard]] static Domain::Result<Application::WorkspaceLocation>
    profileLocationForSetupPath(const Application::FileDialogPath& path);
    [[nodiscard]] const QString& recoveryJournalPath() const noexcept;

private:
    [[nodiscard]] Domain::Result<void> writeRecoveryRecord(
        const Application::InitialSetupRecoveryRecord& record
        ) const;

    Application::WorkspaceCoordinator& m_workspaceCoordinator;
    Application::WorkspaceState& m_workspaceState;
    QString m_recoveryJournalPath;
};

} // namespace ClassMngr::Next::Platform
