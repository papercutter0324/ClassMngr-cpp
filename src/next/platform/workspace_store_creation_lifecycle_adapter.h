#pragma once

#include "next/application/workspace_creation_lifecycle.h"
#include "next/application/workspace_coordinator.h"
#include "next/application/workspace_state.h"

namespace ClassMngr::Next::Platform
{

class WorkspaceCreationFileOperationsPort
{
public:
    virtual ~WorkspaceCreationFileOperationsPort() = default;

    [[nodiscard]] virtual bool profileExists(
        const Application::WorkspaceLocation& location
        ) const = 0;
    [[nodiscard]] virtual Application::WorkspaceLocation recoveryLocationFor(
        const Application::WorkspaceLocation& profile
        ) = 0;
    [[nodiscard]] virtual bool moveProfile(
        const Application::WorkspaceLocation& source,
        const Application::WorkspaceLocation& destination
        ) = 0;
    [[nodiscard]] virtual bool removeProfile(
        const Application::WorkspaceLocation& location
        ) = 0;
};

class QtWorkspaceCreationFileOperationsPort final
    : public WorkspaceCreationFileOperationsPort
{
public:
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
};

// WorkspaceStore-backed normal-create boundary. Platform file operations are
// deliberately separate from the Initial Setup adapter and its journal.
class WorkspaceStoreCreationLifecycleAdapter final
    : public Application::WorkspaceCreationLifecyclePort
{
public:
    WorkspaceStoreCreationLifecycleAdapter(
        Application::WorkspaceCoordinator& workspaceCoordinator,
        Application::WorkspaceState& workspaceState,
        WorkspaceCreationFileOperationsPort& fileOperations
        ) noexcept
        : m_workspaceCoordinator(workspaceCoordinator),
          m_workspaceState(workspaceState),
          m_fileOperations(fileOperations)
    {
    }

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
    [[nodiscard]] Domain::Result<void> closeActiveWorkspace() override;
    [[nodiscard]] Domain::Result<Application::WorkspaceSession>
    createWorkspace(
        const Application::CreateWorkspaceRequest& request
        ) override;

private:
    Application::WorkspaceCoordinator& m_workspaceCoordinator;
    Application::WorkspaceState& m_workspaceState;
    WorkspaceCreationFileOperationsPort& m_fileOperations;
};

} // namespace ClassMngr::Next::Platform
