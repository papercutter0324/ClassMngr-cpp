#pragma once

#include "next/application/selection_state.h"
#include "next/application/workspace_coordinator.h"
#include "next/application/workspace_state.h"
#include "next/application/workspace_use_case.h"

namespace ClassMngr::Next::Application
{

// Owns the long-lived application state and workspace lifecycle services.
// The gateway is borrowed and must outlive this runtime.
class ApplicationRuntime final
{
public:
    explicit ApplicationRuntime(
        WorkspaceGateway& workspaceGateway
        ) noexcept
        : m_workspaceUseCase(workspaceGateway),
          m_workspaceCoordinator(
              m_workspaceUseCase,
              m_workspaceState,
              m_selectionState
              )
    {
    }

    ApplicationRuntime(const ApplicationRuntime&) = delete;
    ApplicationRuntime& operator=(const ApplicationRuntime&) = delete;

    [[nodiscard]] WorkspaceState& workspaceState() noexcept
    {
        return m_workspaceState;
    }

    [[nodiscard]] const WorkspaceState& workspaceState() const noexcept
    {
        return m_workspaceState;
    }

    [[nodiscard]] SelectionState& selectionState() noexcept
    {
        return m_selectionState;
    }

    [[nodiscard]] const SelectionState& selectionState() const noexcept
    {
        return m_selectionState;
    }

    [[nodiscard]] WorkspaceCoordinator& workspaceCoordinator() noexcept
    {
        return m_workspaceCoordinator;
    }

    [[nodiscard]] const WorkspaceCoordinator& workspaceCoordinator() const noexcept
    {
        return m_workspaceCoordinator;
    }

private:
    WorkspaceState m_workspaceState;
    SelectionState m_selectionState;
    WorkspaceUseCase m_workspaceUseCase;
    WorkspaceCoordinator m_workspaceCoordinator;
};

}  // namespace ClassMngr::Next::Application
