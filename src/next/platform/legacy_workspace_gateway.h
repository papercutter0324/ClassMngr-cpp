#pragma once

#include "next/application/workspace_contracts.h"

#include <cassert>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace ClassMngr::Next::Platform
{

// This is the deliberately small error shape exposed by the legacy port. The
// concrete outer adapter may obtain the text from its legacy boundary, but no
// platform error type crosses into this seam.
struct LegacyWorkspaceError final
{
    std::string message;

    friend bool operator==(
        const LegacyWorkspaceError&,
        const LegacyWorkspaceError&
        ) = default;
};

// A value handle lets the gateway pass legacy workspace identity explicitly
// without retaining a mutable service or session object. The identity is
// converted to Domain::WorkspaceId only after the gateway validates it.
class LegacyWorkspaceHandle final
{
public:
    LegacyWorkspaceHandle() = default;

    LegacyWorkspaceHandle(
        std::string workspaceId,
        std::string location
        )
        : m_workspaceId(std::move(workspaceId)),
          m_location(std::move(location))
    {
    }

    [[nodiscard]] const std::string& workspaceId() const noexcept
    {
        return m_workspaceId;
    }

    [[nodiscard]] const std::string& identity() const noexcept
    {
        return m_workspaceId;
    }

    [[nodiscard]] const std::string& location() const noexcept
    {
        return m_location;
    }

    friend bool operator==(
        const LegacyWorkspaceHandle&,
        const LegacyWorkspaceHandle&
        ) = default;

private:
    std::string m_workspaceId;
    std::string m_location;
};

template <typename Value>
class LegacyWorkspaceResult final
{
public:
    [[nodiscard]] static LegacyWorkspaceResult success(
        Value value
        )
    {
        return LegacyWorkspaceResult(std::move(value));
    }

    [[nodiscard]] static LegacyWorkspaceResult failure(
        LegacyWorkspaceError error
        )
    {
        return LegacyWorkspaceResult(std::move(error));
    }

    [[nodiscard]] bool hasValue() const noexcept
    {
        return std::holds_alternative<Value>(m_storage);
    }

    explicit operator bool() const noexcept
    {
        return hasValue();
    }

    [[nodiscard]] const Value& value() const
    {
        assert(hasValue());
        return std::get<Value>(m_storage);
    }

    [[nodiscard]] Value& value()
    {
        assert(hasValue());
        return std::get<Value>(m_storage);
    }

    [[nodiscard]] const LegacyWorkspaceError& error() const
    {
        assert(!hasValue());
        return std::get<LegacyWorkspaceError>(m_storage);
    }

private:
    explicit LegacyWorkspaceResult(
        Value value
        )
        : m_storage(std::move(value))
    {
    }

    explicit LegacyWorkspaceResult(
        LegacyWorkspaceError error
        )
        : m_storage(std::move(error))
    {
    }

    std::variant<Value, LegacyWorkspaceError> m_storage;
};

template <>
class LegacyWorkspaceResult<void> final
{
public:
    [[nodiscard]] static LegacyWorkspaceResult success()
    {
        return LegacyWorkspaceResult(std::monostate{});
    }

    [[nodiscard]] static LegacyWorkspaceResult failure(
        LegacyWorkspaceError error
        )
    {
        return LegacyWorkspaceResult(std::move(error));
    }

    [[nodiscard]] bool hasValue() const noexcept
    {
        return std::holds_alternative<std::monostate>(m_storage);
    }

    explicit operator bool() const noexcept
    {
        return hasValue();
    }

    [[nodiscard]] const LegacyWorkspaceError& error() const
    {
        assert(!hasValue());
        return std::get<LegacyWorkspaceError>(m_storage);
    }

private:
    explicit LegacyWorkspaceResult(
        std::monostate value
        )
        : m_storage(value)
    {
    }

    explicit LegacyWorkspaceResult(
        LegacyWorkspaceError error
        )
        : m_storage(std::move(error))
    {
    }

    std::variant<std::monostate, LegacyWorkspaceError> m_storage;
};

using LegacyWorkspaceStatus = LegacyWorkspaceResult<void>;

// Adapter-neutral shape of the legacy workspace boundary. The returned
// handles and locations are values; the port implementation owns any legacy
// service lifetime and any platform path representation.
class LegacyWorkspacePort
{
public:
    virtual ~LegacyWorkspacePort() = default;

    [[nodiscard]] virtual LegacyWorkspaceResult<LegacyWorkspaceHandle>
    openDatabase(
        const std::string& databasePath
        ) = 0;

    // Creation is intentionally explicit because the legacy boundary has no
    // direct create operation. The concrete adapter owns file preparation or
    // replacement before it performs its legacy open operation.
    [[nodiscard]] virtual LegacyWorkspaceResult<LegacyWorkspaceHandle>
    createOrOpenDatabase(
        const std::string& databasePath
        ) = 0;

    // The handle is an explicit value token for the caller's session. A
    // concrete adapter may ignore it when its legacy close operation has no
    // argument, but it must report the resulting postcondition.
    [[nodiscard]] virtual LegacyWorkspaceStatus closeDatabase(
        const LegacyWorkspaceHandle& handle
        ) = 0;

    [[nodiscard]] virtual bool hasOpenDatabase() const = 0;

    // A copy is intentional: no mutable path or service-owned object may
    // escape through the port.
    [[nodiscard]] virtual std::string currentDatabasePath() const = 0;

    // The legacy save operation is void at its historical boundary. The port
    // returns a status so an outer adapter can report a failed postcondition
    // instead of making the application contract guess.
    [[nodiscard]] virtual LegacyWorkspaceStatus saveDatabase(
        const LegacyWorkspaceHandle& handle
        ) = 0;

    [[nodiscard]] virtual LegacyWorkspaceResult<LegacyWorkspaceHandle>
    saveDatabaseAs(
        const LegacyWorkspaceHandle& handle,
        const std::string& destinationPath
        ) = 0;

    [[nodiscard]] virtual LegacyWorkspaceResult<std::string> exportDatabaseAs(
        const LegacyWorkspaceHandle& handle,
        const std::string& destinationPath
        ) = 0;
};

// Stateless mapping layer from the application workspace contract to the
// legacy port. It owns only the injected port reference and returns value
// sessions/locations; it never retains a current-session snapshot.
class LegacyWorkspaceGateway final : public Application::WorkspaceGateway
{
public:
    explicit LegacyWorkspaceGateway(
        LegacyWorkspacePort& port
        ) noexcept
        : m_port(port)
    {
    }

    LegacyWorkspaceGateway(const LegacyWorkspaceGateway&) = delete;
    LegacyWorkspaceGateway& operator=(const LegacyWorkspaceGateway&) = delete;
    LegacyWorkspaceGateway(LegacyWorkspaceGateway&&) = delete;
    LegacyWorkspaceGateway& operator=(LegacyWorkspaceGateway&&) = delete;

    [[nodiscard]] Domain::Result<Application::WorkspaceSession>
    createWorkspace(
        const Application::CreateWorkspaceRequest& request
        ) override
    {
        return openOrCreateWorkspace(request.location, true);
    }

    [[nodiscard]] Domain::Result<Application::WorkspaceSession>
    openWorkspace(
        const Application::OpenWorkspaceRequest& request
        ) override
    {
        return openOrCreateWorkspace(request.location, false);
    }

    [[nodiscard]] Domain::Result<void> closeWorkspace(
        const Application::CloseWorkspaceRequest& request
        ) override
    {
        const auto handle = handleForSession(request.session);
        if (!handle)
        {
            return Domain::Result<void>::failure(handle.error());
        }

        const LegacyWorkspaceStatus closed =
            m_port.closeDatabase(handle.value());
        if (!closed)
        {
            return Domain::Result<void>::failure(
                mapLegacyError("Closing the workspace", closed.error())
                );
        }

        if (m_port.hasOpenDatabase())
        {
            return Domain::Result<void>::failure(
                postconditionFailure(
                    "Closing the workspace reported success while a workspace remains open."
                    )
                );
        }

        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> saveWorkspace(
        const Application::SaveWorkspaceRequest& request
        ) override
    {
        const auto handle = handleForSession(request.session);
        if (!handle)
        {
            return Domain::Result<void>::failure(handle.error());
        }

        const LegacyWorkspaceStatus saved =
            m_port.saveDatabase(handle.value());
        if (!saved)
        {
            return Domain::Result<void>::failure(
                mapLegacyError("Saving the workspace", saved.error())
                );
        }

        const auto unchanged = validateOpenObservation(
            handle.value(),
            "Saving the workspace"
            );
        if (!unchanged)
        {
            return unchanged;
        }

        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<Application::WorkspaceLocation>
    saveWorkspaceAs(
        const Application::SaveWorkspaceAsRequest& request
        ) override
    {
        const auto handle = handleForSession(request.session);
        if (!handle)
        {
            return Domain::Result<Application::WorkspaceLocation>::failure(
                handle.error()
                );
        }

        if (isBlank(request.destination.value()))
        {
            return Domain::Result<Application::WorkspaceLocation>::failure(
                invalidInput(
                    "Save-as destination must not be empty or whitespace-only."
                    )
                );
        }

        const auto saved = m_port.saveDatabaseAs(
            handle.value(),
            request.destination.value()
            );
        if (!saved)
        {
            return Domain::Result<Application::WorkspaceLocation>::failure(
                mapLegacyError("Saving the workspace as", saved.error())
                );
        }

        const auto returnedId = workspaceIdFromHandle(saved.value());
        if (!returnedId)
        {
            return Domain::Result<Application::WorkspaceLocation>::failure(
                returnedId.error()
                );
        }

        if (
            saved.value().workspaceId()
            != request.session.workspaceId().value()
            )
        {
            return Domain::Result<Application::WorkspaceLocation>::failure(
                invalidInput(
                    "Save-as returned a different workspace identity."
                    )
                );
        }

        const auto unchanged = validateOpenObservation(
            saved.value(),
            "Saving the workspace as"
            );
        if (!unchanged)
        {
            return Domain::Result<Application::WorkspaceLocation>::failure(
                unchanged.error()
                );
        }

        return Domain::Result<Application::WorkspaceLocation>::success(
            Application::WorkspaceLocation(saved.value().location())
            );
    }

    [[nodiscard]] Domain::Result<Application::WorkspaceLocation>
    exportWorkspace(
        const Application::ExportWorkspaceRequest& request
        ) override
    {
        const auto handle = handleForSession(request.session);
        if (!handle)
        {
            return Domain::Result<Application::WorkspaceLocation>::failure(
                handle.error()
                );
        }

        if (isBlank(request.destination.value()))
        {
            return Domain::Result<Application::WorkspaceLocation>::failure(
                invalidInput(
                    "Export destination must not be empty or whitespace-only."
                    )
                );
        }

        const auto exported = m_port.exportDatabaseAs(
            handle.value(),
            request.destination.value()
            );
        if (!exported)
        {
            return Domain::Result<Application::WorkspaceLocation>::failure(
                mapLegacyError("Exporting the workspace", exported.error())
                );
        }

        if (isBlank(exported.value()))
        {
            return Domain::Result<Application::WorkspaceLocation>::failure(
                invalidInput(
                    "Export returned an empty or whitespace-only location."
                    )
                );
        }

        // Export must not replace or close the open workspace. The port
        // observation makes that no-mutation rule testable without storing a
        // gateway-side snapshot.
        const auto unchanged = validateOpenObservation(
            handle.value(),
            "Exporting the workspace"
            );
        if (!unchanged)
        {
            return Domain::Result<Application::WorkspaceLocation>::failure(
                unchanged.error()
                );
        }

        return Domain::Result<Application::WorkspaceLocation>::success(
            Application::WorkspaceLocation(std::move(exported.value()))
            );
    }

private:
    [[nodiscard]] Domain::Result<Application::WorkspaceSession>
    openOrCreateWorkspace(
        const Application::WorkspaceLocation& location,
        const bool create
        )
    {
        if (isBlank(location.value()))
        {
            return Domain::Result<Application::WorkspaceSession>::failure(
                invalidInput(
                    "Workspace location must not be empty or whitespace-only."
                    )
                );
        }

        const auto opened = create
            ? m_port.createOrOpenDatabase(location.value())
            : m_port.openDatabase(location.value());
        if (!opened)
        {
            return Domain::Result<Application::WorkspaceSession>::failure(
                mapLegacyError(
                    create ? "Creating the workspace" : "Opening the workspace",
                    opened.error()
                    )
                );
        }

        return sessionFromHandle(opened.value());
    }

    [[nodiscard]] Domain::Result<LegacyWorkspaceHandle> handleForSession(
        const Application::WorkspaceSession& session
        ) const
    {
        if (
            isBlank(session.workspaceId().value())
            || isBlank(session.location().value())
            )
        {
            return Domain::Result<LegacyWorkspaceHandle>::failure(
                invalidInput(
                    "Workspace session must contain an id and location."
                    )
                );
        }

        return Domain::Result<LegacyWorkspaceHandle>::success(
            LegacyWorkspaceHandle(
                session.workspaceId().value(),
                session.location().value()
                )
            );
    }

    [[nodiscard]] Domain::Result<Application::WorkspaceSession>
    sessionFromHandle(
        const LegacyWorkspaceHandle& handle
        ) const
    {
        const auto workspaceId = workspaceIdFromHandle(handle);
        if (!workspaceId)
        {
            return Domain::Result<Application::WorkspaceSession>::failure(
                workspaceId.error()
                );
        }

        const auto observed = validateOpenObservation(
            handle,
            "Opening the workspace"
            );
        if (!observed)
        {
            return Domain::Result<Application::WorkspaceSession>::failure(
                observed.error()
                );
        }

        return Domain::Result<Application::WorkspaceSession>::success(
            Application::WorkspaceSession(
                workspaceId.value(),
                Application::WorkspaceLocation(handle.location())
                )
            );
    }

    [[nodiscard]] Domain::Result<Domain::WorkspaceId> workspaceIdFromHandle(
        const LegacyWorkspaceHandle& handle
        ) const
    {
        if (
            isBlank(handle.workspaceId())
            || isBlank(handle.location())
            )
        {
            return Domain::Result<Domain::WorkspaceId>::failure(
                invalidInput(
                    "Legacy workspace returned an invalid handle or location."
                    )
                );
        }

        const auto workspaceId = Domain::WorkspaceId::fromString(
            handle.workspaceId()
            );
        if (!workspaceId)
        {
            return Domain::Result<Domain::WorkspaceId>::failure(
                invalidInput(
                    "Legacy workspace returned an invalid workspace identity."
                    )
                );
        }

        return Domain::Result<Domain::WorkspaceId>::success(*workspaceId);
    }

    [[nodiscard]] Domain::Result<void> validateOpenObservation(
        const LegacyWorkspaceHandle& handle,
        const char* operation
        ) const
    {
        if (!m_port.hasOpenDatabase())
        {
            return Domain::Result<void>::failure(
                postconditionFailure(
                    std::string(operation)
                    + " reported success without an open workspace."
                    )
                );
        }

        const std::string observedPath = m_port.currentDatabasePath();
        if (
            isBlank(observedPath)
            || observedPath != handle.location()
            )
        {
            return Domain::Result<void>::failure(
                postconditionFailure(
                    std::string(operation)
                    + " returned a location that does not match the open workspace."
                    )
                );
        }

        return Domain::Result<void>::success();
    }

    [[nodiscard]] static bool isBlank(
        std::string_view value
        ) noexcept
    {
        if (value.empty())
        {
            return true;
        }

        for (const char character : value)
        {
            if (
                std::isspace(
                    static_cast<unsigned char>(character)
                    ) == 0
                )
            {
                return false;
            }
        }

        return true;
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

    [[nodiscard]] static Domain::OperationError postconditionFailure(
        std::string message
        )
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Technical,
            .message = std::move(message),
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError mapLegacyError(
        const char* operation,
        const LegacyWorkspaceError& error
        )
    {
        std::string message = error.message;
        if (message.empty())
        {
            message = std::string(operation) + " failed at the legacy boundary.";
        }

        return Domain::OperationError{
            .code = Domain::ErrorCode::Technical,
            .message = std::move(message),
            .recoverable = false
        };
    }

    LegacyWorkspacePort& m_port;
};

} // namespace ClassMngr::Next::Platform
