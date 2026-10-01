#pragma once

#include "next/application/recent_workspace_history.h"

#include <algorithm>
#include <cstddef>

namespace ClassMngr::Next::Application
{

// Deterministic mutation policy for recent workspace history. Platform path
// normalization and persistence remain at the outer boundary.
class RecentWorkspaceHistoryUseCase final
{
public:
    [[nodiscard]] static RecentWorkspaceHistory record(
        const RecentWorkspaceHistory& current,
        const RecentWorkspacePath& rawPath,
        const RecentWorkspacePath& normalizedPath
        )
    {
        RecentWorkspaceHistory updated = current;
        removeAliases(updated, rawPath, normalizedPath);
        updated.paths.insert(updated.paths.begin(), normalizedPath);
        if (updated.paths.size() > kRecentWorkspaceHistoryMaximumEntries)
        {
            updated.paths.resize(kRecentWorkspaceHistoryMaximumEntries);
        }
        updated.lastPath = normalizedPath;
        return updated;
    }

    [[nodiscard]] static RecentWorkspaceHistory prune(
        const RecentWorkspaceHistory& current,
        const RecentWorkspacePath& rawPath,
        const RecentWorkspacePath& normalizedPath
        )
    {
        RecentWorkspaceHistory updated = current;
        removeAliases(updated, rawPath, normalizedPath);
        if (
            updated.lastPath.has_value()
            && (*updated.lastPath == rawPath
                || *updated.lastPath == normalizedPath)
            )
        {
            updated.lastPath.reset();
        }
        return updated;
    }

private:
    static void removeAliases(
        RecentWorkspaceHistory& history,
        const RecentWorkspacePath& rawPath,
        const RecentWorkspacePath& normalizedPath
        )
    {
        history.paths.erase(
            std::remove_if(
                history.paths.begin(),
                history.paths.end(),
                [&rawPath, &normalizedPath](const RecentWorkspacePath& path)
                {
                    return path == rawPath || path == normalizedPath;
                }
                ),
            history.paths.end()
            );
    }
};

} // namespace ClassMngr::Next::Application
