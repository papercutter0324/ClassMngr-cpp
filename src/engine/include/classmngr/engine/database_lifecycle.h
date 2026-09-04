#pragma once

#include "classmngr/engine/file_system.h"
#include "classmngr/engine/result.h"

#include <string>
#include <string_view>

namespace classmngr::engine
{

// The portable state retained by an adapter while a new database is being
// initialized. Path selection and backup naming remain adapter concerns; the
// lifecycle service owns the file transitions and recovery invariants.
struct DatabaseInitialSetupState
{
    std::string databasePath;
    std::string backupPath;

    [[nodiscard]] bool hasBackup() const noexcept
    {
        return !backupPath.empty();
    }
};

class DatabaseLifecycleService final
{
public:
    explicit DatabaseLifecycleService(
        FileSystem& fileSystem
        );

    // Preserves an existing database at backupPath and returns the state an
    // adapter must retain until finishInitialSetup() or cancelInitialSetup().
    // If databasePath does not exist, the returned state has no backup.
    [[nodiscard]] Result<DatabaseInitialSetupState> beginInitialSetup(
        std::string_view databasePath,
        std::string_view backupPath
        ) const;

    // Removes the preserved original after the new database is accepted.
    [[nodiscard]] Status finishInitialSetup(
        const DatabaseInitialSetupState& state
        ) const;

    // Removes the incomplete database and restores the original. If no
    // original existed, it only removes the incomplete database. The caller
    // supplies a unique sibling path for a temporary incomplete-file move.
    [[nodiscard]] Status cancelInitialSetup(
        const DatabaseInitialSetupState& state,
        std::string_view incompletePath
        ) const;

private:
    [[nodiscard]] Result<DatabaseInitialSetupState> normalizedState(
        const DatabaseInitialSetupState& state
        ) const;

    FileSystem& m_fileSystem;
};

} // namespace classmngr::engine
