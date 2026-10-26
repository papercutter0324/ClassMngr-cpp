#pragma once

#include "next/application/recent_workspace_history.h"
#include "next/application/workspace_contracts.h"

#include <optional>
#include <variant>

namespace ClassMngr::Next::Application
{

enum class StartupWorkspaceSource
{
    CommandLine,
    Recent,
    LastFile
};

enum class StartupNoWorkspaceReason
{
    NoCandidate,
    RecentAutoOpenDisabled,
    StaleRecentPathPruned
};

struct StartupWorkspaceCandidate final
{
    WorkspaceLocation location;
    StartupWorkspaceSource source = StartupWorkspaceSource::Recent;

    friend bool operator==(
        const StartupWorkspaceCandidate&,
        const StartupWorkspaceCandidate&
        ) = default;
};

struct StartupNoWorkspaceCandidate final
{
    StartupNoWorkspaceReason reason = StartupNoWorkspaceReason::NoCandidate;

    friend bool operator==(
        const StartupNoWorkspaceCandidate&,
        const StartupNoWorkspaceCandidate&
        ) = default;
};

using StartupWorkspaceSelection = std::variant<
    StartupNoWorkspaceCandidate,
    StartupWorkspaceCandidate
    >;

// Resolves the legacy launch precedence without performing I/O. A selected
// recent path is checked and, if stale, pruned by the outer adapter; policy
// intentionally never falls through to a second recent path or last-file.
class StartupWorkspacePolicy final
{
public:
    [[nodiscard]] static StartupWorkspaceSelection select(
        const std::optional<WorkspaceLocation>& commandLineLocation,
        const bool recentAutoOpenEnabled,
        const RecentWorkspaceHistory& history
        )
    {
        if (commandLineLocation.has_value()
            && !commandLineLocation->empty())
        {
            return StartupWorkspaceCandidate{
                *commandLineLocation,
                StartupWorkspaceSource::CommandLine
            };
        }

        if (!recentAutoOpenEnabled)
        {
            return StartupNoWorkspaceCandidate{
                StartupNoWorkspaceReason::RecentAutoOpenDisabled
            };
        }

        if (!history.paths.empty())
        {
            if (history.paths.front().empty())
            {
                return StartupNoWorkspaceCandidate{};
            }

            return StartupWorkspaceCandidate{
                history.paths.front(),
                StartupWorkspaceSource::Recent
            };
        }

        if (history.lastPath.has_value() && !history.lastPath->empty())
        {
            return StartupWorkspaceCandidate{
                *history.lastPath,
                StartupWorkspaceSource::LastFile
            };
        }

        return StartupNoWorkspaceCandidate{};
    }
};

struct StartupWorkspaceNotOpened final
{
    StartupNoWorkspaceReason reason = StartupNoWorkspaceReason::NoCandidate;
    std::optional<Domain::OperationError> historyError;

    friend bool operator==(
        const StartupWorkspaceNotOpened&,
        const StartupWorkspaceNotOpened&
        ) = default;
};

struct StartupWorkspaceOpened final
{
    WorkspaceSession session;
    std::optional<Domain::OperationError> historyError;

    friend bool operator==(
        const StartupWorkspaceOpened&,
        const StartupWorkspaceOpened&
        ) = default;
};

struct StartupWorkspaceOpenFailed final
{
    Domain::OperationError error;

    friend bool operator==(
        const StartupWorkspaceOpenFailed&,
        const StartupWorkspaceOpenFailed&
        ) = default;
};

// No workspace chosen and a workspace open error are different outcomes.
// Both allow the empty shell to start, while open failures retain their typed
// domain error for diagnostics or a later startup surface.
using StartupWorkspaceOutcome = std::variant<
    StartupWorkspaceNotOpened,
    StartupWorkspaceOpened,
    StartupWorkspaceOpenFailed
    >;

} // namespace ClassMngr::Next::Application
