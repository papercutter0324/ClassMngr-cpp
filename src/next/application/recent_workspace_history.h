#pragma once

#include "next/application/workspace_contracts.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace ClassMngr::Next::Application
{

// Recent workspace paths use the same adapter-neutral UTF-8 path value as the
// workspace boundary. The Qt-facing representation stays in the outer
// adapter, while this contract carries neither QString nor QStringList.
using RecentWorkspacePath = WorkspacePath;
using RecentWorkspacePaths = std::vector<RecentWorkspacePath>;

inline constexpr std::size_t kRecentWorkspaceHistoryMaximumEntries = 10;

// A persistence snapshot keeps the recent list and the legacy last-file
// fallback together. An absent last path represents the legacy empty or
// unavailable value without exposing a Qt storage type.
struct RecentWorkspaceHistory final
{
    RecentWorkspacePaths paths;
    std::optional<RecentWorkspacePath> lastPath;

    [[nodiscard]] std::optional<RecentWorkspacePath>
    mostRecentDatabasePath() const
    {
        if (!paths.empty())
        {
            return paths.front();
        }

        return lastPath;
    }

    friend bool operator==(
        const RecentWorkspaceHistory&,
        const RecentWorkspaceHistory&
        ) = default;
};

// Persistence remains void because the legacy SettingsManager operations do
// not report a status. The outer adapter owns all Qt and legacy conversion.
class RecentWorkspaceHistoryPort
{
public:
    virtual ~RecentWorkspaceHistoryPort() = default;

    [[nodiscard]] virtual RecentWorkspaceHistory load() const = 0;

    virtual void save(
        const RecentWorkspaceHistory& history
        ) const = 0;

    virtual void clear() const = 0;
};

} // namespace ClassMngr::Next::Application
