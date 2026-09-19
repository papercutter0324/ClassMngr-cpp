#pragma once

#include "next/application/workspace_contracts.h"

#include <algorithm>
#include <cctype>

namespace ClassMngr::Next::Application
{

// Stateless orchestration boundary for the initial workspace lifecycle.
// Session ownership stays with the caller through WorkspaceSession.
class WorkspaceUseCase final
{
public:
    explicit WorkspaceUseCase(
        WorkspaceGateway& gateway
        ) noexcept
        : m_gateway(gateway)
    {
    }

    [[nodiscard]] Domain::Result<WorkspaceSession> createWorkspace(
        const CreateWorkspaceRequest& request
        ) const
    {
        if (isBlank(request.location))
        {
            return Domain::Result<WorkspaceSession>::failure(
                invalidInput(
                    "Workspace location must not be empty or whitespace-only."
                    )
                );
        }

        return m_gateway.createWorkspace(request);
    }

    [[nodiscard]] Domain::Result<WorkspaceSession> openWorkspace(
        const OpenWorkspaceRequest& request
        ) const
    {
        if (isBlank(request.location))
        {
            return Domain::Result<WorkspaceSession>::failure(
                invalidInput(
                    "Workspace location must not be empty or whitespace-only."
                    )
                );
        }

        return m_gateway.openWorkspace(request);
    }

    [[nodiscard]] Domain::Result<void> closeWorkspace(
        const CloseWorkspaceRequest& request
        ) const
    {
        if (!isValid(request.session))
        {
            return Domain::Result<void>::failure(
                invalidInput("Workspace session must contain an id and location.")
                );
        }

        return m_gateway.closeWorkspace(request);
    }

    [[nodiscard]] Domain::Result<void> saveWorkspace(
        const SaveWorkspaceRequest& request
        ) const
    {
        if (!isValid(request.session))
        {
            return Domain::Result<void>::failure(
                invalidInput("Workspace session must contain an id and location.")
                );
        }

        return m_gateway.saveWorkspace(request);
    }

    [[nodiscard]] Domain::Result<WorkspaceLocation> saveWorkspaceAs(
        const SaveWorkspaceAsRequest& request
        ) const
    {
        if (!isValid(request.session) || isBlank(request.destination))
        {
            return Domain::Result<WorkspaceLocation>::failure(
                invalidInput(
                    "Workspace session and save-as destination must be valid."
                    )
                );
        }

        return m_gateway.saveWorkspaceAs(request);
    }

    [[nodiscard]] Domain::Result<WorkspaceLocation> exportWorkspace(
        const ExportWorkspaceRequest& request
        ) const
    {
        if (!isValid(request.session) || isBlank(request.destination))
        {
            return Domain::Result<WorkspaceLocation>::failure(
                invalidInput(
                    "Workspace session and export destination must be valid."
                    )
                );
        }

        return m_gateway.exportWorkspace(request);
    }

private:
    [[nodiscard]] static bool isValid(
        const WorkspaceSession& session
        ) noexcept
    {
        return !session.workspaceId().value().empty()
            && !isBlank(session.location());
    }

    [[nodiscard]] static bool isBlank(
        const WorkspaceLocation& location
        ) noexcept
    {
        return location.value().empty()
            || std::all_of(
                location.value().cbegin(),
                location.value().cend(),
                [](const char character)
                {
                    return std::isspace(
                        static_cast<unsigned char>(character)
                        ) != 0;
                }
                );
    }

    [[nodiscard]] static Domain::OperationError invalidInput(
        const char* message
        )
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::InvalidInput,
            .message = message,
            .recoverable = false
        };
    }

    WorkspaceGateway& m_gateway;
};

}
