#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

// Adapter-neutral workspace location. Conversion to a platform or UI path
// type belongs outside this contract.
class WorkspaceLocation final
{
public:
    WorkspaceLocation() = default;

    explicit WorkspaceLocation(
        std::string value
        )
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_value.empty();
    }

    [[nodiscard]] const std::string& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const WorkspaceLocation&,
        const WorkspaceLocation&
        ) = default;

private:
    std::string m_value;
};

using WorkspacePath = WorkspaceLocation;

// The caller owns this copyable projection and supplies it back to close.
// The application use case does not retain a current session.
class WorkspaceSession final
{
public:
    WorkspaceSession(
        Domain::WorkspaceId workspaceId,
        WorkspaceLocation location
        )
        : m_workspaceId(std::move(workspaceId)),
          m_location(std::move(location))
    {
    }

    [[nodiscard]] const Domain::WorkspaceId& workspaceId() const noexcept
    {
        return m_workspaceId;
    }

    [[nodiscard]] const WorkspaceLocation& location() const noexcept
    {
        return m_location;
    }

    friend bool operator==(
        const WorkspaceSession&,
        const WorkspaceSession&
        ) = default;

private:
    Domain::WorkspaceId m_workspaceId;
    WorkspaceLocation m_location;
};

struct CreateWorkspaceRequest final
{
    WorkspaceLocation location;
};

struct OpenWorkspaceRequest final
{
    WorkspaceLocation location;
};

struct CloseWorkspaceRequest final
{
    WorkspaceSession session;
};

/*
 * This is the adapter-facing seam for workspace persistence. An outer
 * adapter performs platform-path conversion and maps text-based boundary
 * errors to Domain::OperationError; this header intentionally has no Qt or
 * legacy dependency.
 */
class WorkspaceGateway
{
public:
    virtual ~WorkspaceGateway() = default;

    [[nodiscard]] virtual Domain::Result<WorkspaceSession> createWorkspace(
        const CreateWorkspaceRequest& request
        ) = 0;

    [[nodiscard]] virtual Domain::Result<WorkspaceSession> openWorkspace(
        const OpenWorkspaceRequest& request
        ) = 0;

    [[nodiscard]] virtual Domain::Result<void> closeWorkspace(
        const CloseWorkspaceRequest& request
        ) = 0;
};

}
