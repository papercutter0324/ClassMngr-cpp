#pragma once

#include "next/application/selection_state.h"
#include "next/application/workspace_state.h"
#include "next/application/workspace_use_case.h"

namespace ClassMngr::Next::Application
{

// Synchronous owner of the workspace lifecycle boundary. Gateway calls happen
// only after state guards pass, and application state is committed only after
// the gateway reports success.
class WorkspaceCoordinator final
{
public:
    explicit WorkspaceCoordinator(
        const WorkspaceUseCase& workspaceUseCase,
        WorkspaceState& workspaceState,
        SelectionState& selectionState
        ) noexcept
        : m_workspaceUseCase(workspaceUseCase),
          m_workspaceState(workspaceState),
          m_selectionState(selectionState)
    {
    }

    [[nodiscard]] Domain::Result<WorkspaceSession> createWorkspace(
        const CreateWorkspaceRequest& request
        )
    {
        if (isDirty())
        {
            return Domain::Result<WorkspaceSession>::failure(
                dirtyReplacementConflict()
                );
        }

        const auto result = m_workspaceUseCase.createWorkspace(request);
        if (!result)
        {
            return result;
        }

        const auto stateResult = m_workspaceState.open(result.value());
        if (!stateResult)
        {
            return Domain::Result<WorkspaceSession>::failure(
                stateResult.error()
                );
        }

        m_selectionState.clear();
        return result;
    }

    [[nodiscard]] Domain::Result<WorkspaceSession> openWorkspace(
        const OpenWorkspaceRequest& request
        )
    {
        if (isDirty())
        {
            return Domain::Result<WorkspaceSession>::failure(
                dirtyReplacementConflict()
                );
        }

        const auto result = m_workspaceUseCase.openWorkspace(request);
        if (!result)
        {
            return result;
        }

        const auto stateResult = m_workspaceState.open(result.value());
        if (!stateResult)
        {
            return Domain::Result<WorkspaceSession>::failure(
                stateResult.error()
                );
        }

        m_selectionState.clear();
        return result;
    }

    [[nodiscard]] Domain::Result<void> closeWorkspace()
    {
        const WorkspaceStateSnapshot beforeClose = m_workspaceState.snapshot();
        if (!beforeClose.session().has_value())
        {
            return Domain::Result<void>::failure(
                notFound()
                );
        }

        if (beforeClose.unsavedState() == WorkspaceUnsavedState::Dirty)
        {
            return Domain::Result<void>::failure(
                dirtyCloseConflict()
                );
        }

        const auto result = m_workspaceUseCase.closeWorkspace(
            CloseWorkspaceRequest{*beforeClose.session()}
            );
        if (!result)
        {
            return result;
        }

        const auto stateResult = m_workspaceState.close();
        if (!stateResult)
        {
            return stateResult;
        }

        m_selectionState.clear();
        return result;
    }

private:
    [[nodiscard]] bool isDirty() const
    {
        const WorkspaceStateSnapshot snapshot = m_workspaceState.snapshot();
        return snapshot.unsavedState() == WorkspaceUnsavedState::Dirty;
    }

    [[nodiscard]] static Domain::OperationError dirtyReplacementConflict()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Conflict,
            .message = "Cannot replace a workspace with unsaved changes.",
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError dirtyCloseConflict()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Conflict,
            .message = "Cannot close a workspace with unsaved changes.",
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError notFound()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::NotFound,
            .message = "No workspace session is open.",
            .recoverable = false
        };
    }

    const WorkspaceUseCase& m_workspaceUseCase;
    WorkspaceState& m_workspaceState;
    SelectionState& m_selectionState;
};

} // namespace ClassMngr::Next::Application
