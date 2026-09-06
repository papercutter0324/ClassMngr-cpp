#include "classmngr/engine/database_lifecycle.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace classmngr::engine
{
namespace
{

Error invalidPath(
    std::string_view token
    )
{
    return {
        ErrorCode::InvalidArgument,
        std::string(token),
        std::nullopt
    };
}

} // namespace

DatabaseLifecycleService::DatabaseLifecycleService(
    FileSystem& fileSystem
    )
    : m_fileSystem(fileSystem)
{
}

Result<DatabaseInitialSetupState>
DatabaseLifecycleService::normalizedState(
    const DatabaseInitialSetupState& state
    ) const
{
    const Result<std::string> databasePath = m_fileSystem.normalizePath(
        state.databasePath
        );
    if (!databasePath)
    {
        return std::unexpected(databasePath.error());
    }

    DatabaseInitialSetupState normalized;
    normalized.databasePath = *databasePath;

    if (!state.backupPath.empty())
    {
        const Result<std::string> backupPath = m_fileSystem.normalizePath(
            state.backupPath
            );
        if (!backupPath)
        {
            return std::unexpected(backupPath.error());
        }
        if (*backupPath == normalized.databasePath)
        {
            return std::unexpected(invalidPath(
                FileSystemErrorToken::MoveFailed
                ));
        }
        normalized.backupPath = *backupPath;
    }

    return normalized;
}

Result<DatabaseInitialSetupState>
DatabaseLifecycleService::beginInitialSetup(
    std::string_view databasePath,
    std::string_view backupPath
    ) const
{
    const Result<std::string> normalizedDatabasePath =
        m_fileSystem.normalizePath(databasePath);
    if (!normalizedDatabasePath)
    {
        return std::unexpected(normalizedDatabasePath.error());
    }

    const Result<bool> databaseExists = m_fileSystem.exists(
        *normalizedDatabasePath
        );
    if (!databaseExists)
    {
        return std::unexpected(databaseExists.error());
    }

    DatabaseInitialSetupState state;
    state.databasePath = *normalizedDatabasePath;
    if (!*databaseExists)
    {
        return state;
    }

    const Result<std::string> normalizedBackupPath =
        m_fileSystem.normalizePath(backupPath);
    if (!normalizedBackupPath)
    {
        return std::unexpected(normalizedBackupPath.error());
    }
    if (*normalizedBackupPath == state.databasePath)
    {
        return std::unexpected(invalidPath(
            FileSystemErrorToken::MoveFailed
            ));
    }

    const Result<bool> backupExists = m_fileSystem.exists(
        *normalizedBackupPath
        );
    if (!backupExists)
    {
        return std::unexpected(backupExists.error());
    }
    if (*backupExists)
    {
        return std::unexpected(invalidPath(
            FileSystemErrorToken::MoveFailed
            ));
    }

    const Status preserved = m_fileSystem.moveFile(
        state.databasePath,
        *normalizedBackupPath
        );
    if (!preserved)
    {
        return std::unexpected(preserved.error());
    }

    state.backupPath = *normalizedBackupPath;
    return state;
}

Status DatabaseLifecycleService::finishInitialSetup(
    const DatabaseInitialSetupState& state
    ) const
{
    const Result<DatabaseInitialSetupState> normalized = normalizedState(state);
    if (!normalized)
    {
        return std::unexpected(normalized.error());
    }

    if (!normalized->hasBackup())
    {
        return {};
    }

    return m_fileSystem.removeFile(normalized->backupPath);
}

Status DatabaseLifecycleService::cancelInitialSetup(
    const DatabaseInitialSetupState& state,
    std::string_view incompletePath
    ) const
{
    const Result<DatabaseInitialSetupState> normalized = normalizedState(state);
    if (!normalized)
    {
        return std::unexpected(normalized.error());
    }

    if (!normalized->hasBackup())
    {
        return m_fileSystem.removeFile(normalized->databasePath);
    }

    const Result<std::string> normalizedIncompletePath =
        m_fileSystem.normalizePath(incompletePath);
    if (!normalizedIncompletePath)
    {
        return std::unexpected(normalizedIncompletePath.error());
    }
    if (
        *normalizedIncompletePath == normalized->databasePath
        || *normalizedIncompletePath == normalized->backupPath
        )
    {
        return std::unexpected(invalidPath(
            FileSystemErrorToken::MoveFailed
            ));
    }

    const Result<bool> databaseExists = m_fileSystem.exists(
        normalized->databasePath
        );
    if (!databaseExists)
    {
        return std::unexpected(databaseExists.error());
    }

    bool movedIncomplete = false;
    if (*databaseExists)
    {
        const Status moved = m_fileSystem.moveFile(
            normalized->databasePath,
            *normalizedIncompletePath
            );
        if (!moved)
        {
            return std::unexpected(moved.error());
        }
        movedIncomplete = true;
    }

    const Status restored = m_fileSystem.moveFile(
        normalized->backupPath,
        normalized->databasePath
        );
    if (!restored)
    {
        if (movedIncomplete)
        {
            // Best-effort rollback keeps the new database visible if the
            // original cannot be restored to its destination.
            (void) m_fileSystem.moveFile(
                *normalizedIncompletePath,
                normalized->databasePath
                );
        }
        return std::unexpected(restored.error());
    }

    if (movedIncomplete)
    {
        const Status cleaned = m_fileSystem.removeFile(
            *normalizedIncompletePath
            );
        if (!cleaned)
        {
            // The original database is already restored. Report cleanup
            // failure so the adapter can surface the leftover artifact.
            return std::unexpected(cleaned.error());
        }
    }

    return {};
}

} // namespace classmngr::engine
