#pragma once

#include "next/application/workspace_contracts.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <utility>

namespace ClassMngr::Next::Application
{

enum class WorkspaceLifecycleState
{
    Closed,
    Open
};

enum class WorkspaceUnsavedState
{
    Clean,
    Dirty
};

// A value snapshot of the application-owned workspace state. The lifecycle is
// derived from the optional session so a closed snapshot cannot carry a stale
// session or unsaved changes.
class WorkspaceStateSnapshot final
{
public:
    WorkspaceStateSnapshot() = default;

    explicit WorkspaceStateSnapshot(
        WorkspaceSession session,
        WorkspaceUnsavedState unsavedState = WorkspaceUnsavedState::Clean
        )
        : m_session(std::move(session)),
          m_unsavedState(unsavedState)
    {
    }

    [[nodiscard]] WorkspaceLifecycleState lifecycle() const noexcept
    {
        return m_session.has_value()
            ? WorkspaceLifecycleState::Open
            : WorkspaceLifecycleState::Closed;
    }

    [[nodiscard]] const std::optional<WorkspaceSession>& session() const noexcept
    {
        return m_session;
    }

    [[nodiscard]] WorkspaceUnsavedState unsavedState() const noexcept
    {
        return m_unsavedState;
    }

    friend bool operator==(
        const WorkspaceStateSnapshot&,
        const WorkspaceStateSnapshot&
        ) = default;

private:
    friend class WorkspaceState;

    std::optional<WorkspaceSession> m_session;
    WorkspaceUnsavedState m_unsavedState = WorkspaceUnsavedState::Clean;
};

// Owns one explicit workspace-state snapshot. Opening a session while another
// clean session is open replaces it; dirty replacement and dirty close are
// rejected so the caller must save or otherwise resolve the pending changes.
class WorkspaceState final
{
public:
    WorkspaceState() = default;

    [[nodiscard]] WorkspaceStateSnapshot snapshot() const
    {
        return m_snapshot;
    }

    [[nodiscard]] Domain::Result<void> open(
        WorkspaceSession session
        )
    {
        if (!isValid(session))
        {
            return Domain::Result<void>::failure(
                invalidInput(
                    "Workspace session must contain an id and location."
                    )
                );
        }

        if (m_snapshot.unsavedState() == WorkspaceUnsavedState::Dirty)
        {
            return Domain::Result<void>::failure(
                conflict(
                    "Cannot replace a workspace with unsaved changes."
                    )
                );
        }

        m_snapshot = WorkspaceStateSnapshot(std::move(session));
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> markDirty()
    {
        if (!m_snapshot.session().has_value())
        {
            return Domain::Result<void>::failure(
                notFound("No workspace session is open.")
                );
        }

        m_snapshot.m_unsavedState = WorkspaceUnsavedState::Dirty;
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> markSaved()
    {
        if (!m_snapshot.session().has_value())
        {
            return Domain::Result<void>::failure(
                notFound("No workspace session is open.")
                );
        }

        m_snapshot.m_unsavedState = WorkspaceUnsavedState::Clean;
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> close()
    {
        if (!m_snapshot.session().has_value())
        {
            return Domain::Result<void>::failure(
                notFound("No workspace session is open.")
                );
        }

        if (m_snapshot.unsavedState() == WorkspaceUnsavedState::Dirty)
        {
            return Domain::Result<void>::failure(
                conflict(
                    "Cannot close a workspace with unsaved changes."
                    )
                );
        }

        m_snapshot = WorkspaceStateSnapshot{};
        return Domain::Result<void>::success();
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

    [[nodiscard]] static Domain::OperationError notFound(
        const char* message
        )
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::NotFound,
            .message = message,
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError conflict(
        const char* message
        )
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Conflict,
            .message = message,
            .recoverable = true
        };
    }

    WorkspaceStateSnapshot m_snapshot;
};

}
