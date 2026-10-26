#pragma once

#include "next/application/personal_details_save.h"
#include "next/application/workspace_contracts.h"

#include <memory>

class WorkspaceStore;

namespace ClassMngr::Next::Platform
{

// Store-backed persistence boundary for the Next startup workspace. The
// adapter borrows the single WorkspaceStore owned by the composition root.
class WorkspaceStoreGateway final : public Application::WorkspaceGateway
{
public:
    WorkspaceStoreGateway();
    ~WorkspaceStoreGateway() override;

    WorkspaceStoreGateway(const WorkspaceStoreGateway&) = delete;
    WorkspaceStoreGateway& operator=(const WorkspaceStoreGateway&) = delete;

    [[nodiscard]] Domain::Result<Application::WorkspaceSession>
    createWorkspace(
        const Application::CreateWorkspaceRequest& request
        ) override;

    [[nodiscard]] Domain::Result<Application::WorkspaceSession>
    openWorkspace(
        const Application::OpenWorkspaceRequest& request
        ) override;

    [[nodiscard]] Domain::Result<void> closeWorkspace(
        const Application::CloseWorkspaceRequest& request
        ) override;

    [[nodiscard]] Domain::Result<void> saveWorkspace(
        const Application::SaveWorkspaceRequest& request
        ) override;

    [[nodiscard]] Domain::Result<Application::WorkspaceLocation>
    saveWorkspaceAs(
        const Application::SaveWorkspaceAsRequest& request
        ) override;

    [[nodiscard]] Domain::Result<Application::WorkspaceLocation>
    exportWorkspace(
        const Application::ExportWorkspaceRequest& request
        ) override;

    // Setup feature operations share the active store owned by this gateway.
    [[nodiscard]] Application::PersonalDetailsReadResult
    readPersonalDetails() const;
    [[nodiscard]] Application::PersonalDetailsSaveResult
    savePersonalDetails(
        const Application::PersonalDetailsSaveRequest& request
        ) const;

private:
    [[nodiscard]] Domain::Result<Application::WorkspaceSession> sessionForOpenStore() const;
    [[nodiscard]] Domain::Result<void> validateSession(
        const Application::WorkspaceSession& session
        ) const;

    std::unique_ptr<WorkspaceStore> m_store;
};

} // namespace ClassMngr::Next::Platform
