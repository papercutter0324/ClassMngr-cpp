#include "classmngr/engine/database_lifecycle.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace fs = std::filesystem;
using classmngr::engine::DatabaseInitialSetupState;
using classmngr::engine::DatabaseLifecycleService;
using classmngr::engine::ErrorCode;
using classmngr::engine::FileSystem;
using classmngr::engine::Result;
using classmngr::engine::Status;
using classmngr::engine::StandardFileSystem;

namespace
{

std::string pathToUtf8(
    const fs::path& path
    )
{
    const std::u8string encoded = path.generic_u8string();
    return {
        reinterpret_cast<const char*>(encoded.data()),
        encoded.size()
    };
}

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineDatabaseLifecycleTests: "
              << message
              << '\n';
    return false;
}

template<class T>
bool hasErrorToken(
    const Result<T>& result,
    ErrorCode code,
    std::string_view token
    )
{
    return !result
        && result.error().code == code
        && result.error().message == token;
}

bool hasBytes(
    const FileSystem& fileSystem,
    std::string_view path,
    std::string_view expected
    )
{
    const auto bytes = fileSystem.readBytes(path);
    return bytes && *bytes == expected;
}

class FailureInjectingFileSystem final : public FileSystem
{
public:
    void failMoveOnCall(int call) noexcept
    {
        m_failMoveCall = call;
    }

    void failRemoveOnCall(int call) noexcept
    {
        m_failRemoveCall = call;
    }

    StandardFileSystem& delegate() noexcept
    {
        return m_delegate;
    }

    Result<std::string> normalizePath(
        std::string_view utf8Path
        ) const override
    {
        return m_delegate.normalizePath(utf8Path);
    }

    Result<bool> exists(
        std::string_view utf8Path
        ) const override
    {
        return m_delegate.exists(utf8Path);
    }

    Status createDirectories(
        std::string_view utf8DirectoryPath
        ) const override
    {
        return m_delegate.createDirectories(utf8DirectoryPath);
    }

    Result<std::string> readBytes(
        std::string_view utf8Path
        ) const override
    {
        return m_delegate.readBytes(utf8Path);
    }

    Status writeBytes(
        std::string_view utf8Path,
        std::string_view bytes,
        bool createParentDirectories = false
        ) const override
    {
        return m_delegate.writeBytes(
            utf8Path,
            bytes,
            createParentDirectories
            );
    }

    Status copyFile(
        std::string_view utf8SourcePath,
        std::string_view utf8DestinationPath,
        bool createParentDirectories = false
        ) const override
    {
        return m_delegate.copyFile(
            utf8SourcePath,
            utf8DestinationPath,
            createParentDirectories
            );
    }

    Status moveFile(
        std::string_view utf8SourcePath,
        std::string_view utf8DestinationPath
        ) const override
    {
        ++m_moveCalls;
        if (m_moveCalls == m_failMoveCall)
        {
            return injectedFailure("test.move-failed");
        }
        return m_delegate.moveFile(
            utf8SourcePath,
            utf8DestinationPath
            );
    }

    Status replaceFileAtomically(
        std::string_view utf8TemporaryPath,
        std::string_view utf8DestinationPath
        ) const override
    {
        return m_delegate.replaceFileAtomically(
            utf8TemporaryPath,
            utf8DestinationPath
            );
    }

    Status replaceDirectoryAtomically(
        std::string_view utf8TemporaryDirectoryPath,
        std::string_view utf8DestinationDirectoryPath
        ) const override
    {
        return m_delegate.replaceDirectoryAtomically(
            utf8TemporaryDirectoryPath,
            utf8DestinationDirectoryPath
            );
    }

    Status removeFile(
        std::string_view utf8Path
        ) const override
    {
        ++m_removeCalls;
        if (m_removeCalls == m_failRemoveCall)
        {
            return injectedFailure("test.remove-failed");
        }
        return m_delegate.removeFile(utf8Path);
    }

    Result<std::string> createTemporaryDirectory(
        std::string_view utf8ParentDirectory
        ) const override
    {
        return m_delegate.createTemporaryDirectory(utf8ParentDirectory);
    }

    Status removeTemporaryDirectory(
        std::string_view utf8DirectoryPath
        ) const override
    {
        return m_delegate.removeTemporaryDirectory(utf8DirectoryPath);
    }

private:
    static Status injectedFailure(std::string_view token)
    {
        return std::unexpected(classmngr::engine::Error{
            ErrorCode::Io,
            std::string(token),
            std::nullopt
        });
    }

    StandardFileSystem m_delegate;
    int m_failMoveCall = -1;
    int m_failRemoveCall = -1;
    mutable int m_moveCalls = 0;
    mutable int m_removeCalls = 0;
};

} // namespace

int main()
{
    bool passed = true;
    StandardFileSystem fileSystem;
    const auto temporaryRoot = fileSystem.createTemporaryDirectory(
        pathToUtf8(fs::temp_directory_path())
        );
    passed &= expect(
        temporaryRoot.has_value(),
        "temporary root could not be created"
        );
    if (!temporaryRoot)
    {
        return 1;
    }

    const fs::path root = fs::u8path(*temporaryRoot);
    const std::string databasePath = pathToUtf8(root / "profile.tps");
    const std::string backupPath = pathToUtf8(root / "profile.backup");
    const std::string incompletePath = pathToUtf8(root / "profile.incomplete");
    DatabaseLifecycleService lifecycle(fileSystem);

    passed &= expect(
        fileSystem.writeBytes(databasePath, "original profile").has_value(),
        "original database fixture could not be written"
        );

    const auto started = lifecycle.beginInitialSetup(
        databasePath,
        backupPath
        );
    passed &= expect(
        started && started->hasBackup(),
        "beginInitialSetup did not preserve an existing database"
        );
    const auto originalExists = fileSystem.exists(databasePath);
    const auto backupExists = fileSystem.exists(backupPath);
    passed &= expect(
        originalExists && !*originalExists
            && backupExists && *backupExists
            && hasBytes(fileSystem, backupPath, "original profile"),
        "beginInitialSetup left the original database in the wrong state"
        );

    passed &= expect(
        fileSystem.writeBytes(databasePath, "new profile").has_value(),
        "new database fixture could not be written"
        );
    if (started)
    {
        passed &= expect(
            lifecycle.finishInitialSetup(*started).has_value(),
            "finishInitialSetup failed"
            );
    }
    const auto backupAfterFinish = fileSystem.exists(backupPath);
    passed &= expect(
        backupAfterFinish && !*backupAfterFinish
            && hasBytes(fileSystem, databasePath, "new profile"),
        "finishInitialSetup did not finalize the new database"
        );

    passed &= expect(
        fileSystem.writeBytes(databasePath, "original before cancel").has_value(),
        "cancel fixture could not restore the original database"
        );
    const auto cancelStarted = lifecycle.beginInitialSetup(
        databasePath,
        backupPath
        );
    passed &= expect(
        cancelStarted && cancelStarted->hasBackup(),
        "cancel setup could not preserve the original database"
        );
    passed &= expect(
        fileSystem.writeBytes(databasePath, "incomplete profile").has_value(),
        "incomplete database fixture could not be written"
        );
    if (cancelStarted)
    {
        passed &= expect(
            lifecycle.cancelInitialSetup(
                *cancelStarted,
                incompletePath
                ).has_value(),
            "cancelInitialSetup failed to restore the original database"
            );
    }
    const auto backupAfterCancel = fileSystem.exists(backupPath);
    const auto incompleteAfterCancel = fileSystem.exists(incompletePath);
    passed &= expect(
        backupAfterCancel && !*backupAfterCancel
            && incompleteAfterCancel && !*incompleteAfterCancel
            && hasBytes(fileSystem, databasePath, "original before cancel"),
        "cancelInitialSetup left recovery artifacts or the wrong database"
        );

    const std::string newDatabasePath = pathToUtf8(root / "new-profile.tps");
    const auto newSetup = lifecycle.beginInitialSetup(
        newDatabasePath,
        pathToUtf8(root / "new-profile.backup")
        );
    passed &= expect(
        newSetup && !newSetup->hasBackup(),
        "beginInitialSetup incorrectly created a backup for a new database"
        );
    passed &= expect(
        fileSystem.writeBytes(newDatabasePath, "incomplete new profile").has_value(),
        "new-database cancellation fixture could not be written"
        );
    if (newSetup)
    {
        passed &= expect(
            lifecycle.cancelInitialSetup(*newSetup, "").has_value(),
            "cancelInitialSetup failed for a database without an original"
            );
    }
    const auto newDatabaseExists = fileSystem.exists(newDatabasePath);
    passed &= expect(
        newDatabaseExists && !*newDatabaseExists,
        "cancelInitialSetup left an incomplete new database"
        );

    passed &= expect(
        fileSystem.writeBytes(databasePath, "collision original").has_value()
            && fileSystem.writeBytes(backupPath, "occupied backup").has_value(),
        "collision fixture could not be written"
        );
    const auto collision = lifecycle.beginInitialSetup(
        databasePath,
        backupPath
        );
    passed &= expect(
        hasErrorToken(
            collision,
            ErrorCode::InvalidArgument,
            "file-system.move-failed"
            )
            && hasBytes(fileSystem, databasePath, "collision original")
            && hasBytes(fileSystem, backupPath, "occupied backup"),
        "occupied backup path did not preserve both source and destination"
        );

    const std::string backupFailureDatabase = pathToUtf8(
        root / "backup-failure.tps"
        );
    const std::string backupFailurePath = pathToUtf8(
        root / "backup-failure.backup"
        );
    FailureInjectingFileSystem backupFailureFileSystem;
    passed &= expect(
        backupFailureFileSystem.delegate().writeBytes(
            backupFailureDatabase,
            "backup failure original"
            ).has_value(),
        "backup failure fixture could not be written"
        );
    backupFailureFileSystem.failMoveOnCall(1);
    const auto failedBackup = DatabaseLifecycleService(
        backupFailureFileSystem
        ).beginInitialSetup(
            backupFailureDatabase,
            backupFailurePath
            );
    passed &= expect(
        !failedBackup
            && hasBytes(
                backupFailureFileSystem,
                backupFailureDatabase,
                "backup failure original"
                )
            && !backupFailureFileSystem.delegate().exists(backupFailurePath)
                .value_or(true),
        "backup failure did not preserve the source database"
        );

    const std::string finishFailureDatabase = pathToUtf8(
        root / "finish-failure.tps"
        );
    const std::string finishFailurePath = pathToUtf8(
        root / "finish-failure.backup"
        );
    FailureInjectingFileSystem finishFailureFileSystem;
    passed &= expect(
        finishFailureFileSystem.delegate().writeBytes(
            finishFailureDatabase,
            "finish failure original"
            ).has_value(),
        "finish failure fixture could not be written"
        );
    const auto finishSetup = DatabaseLifecycleService(
        finishFailureFileSystem
        ).beginInitialSetup(
            finishFailureDatabase,
            finishFailurePath
            );
    passed &= expect(
        finishSetup
            && finishFailureFileSystem.delegate().writeBytes(
                finishFailureDatabase,
                "finish failure new"
                ).has_value(),
        "finish failure setup could not be prepared"
        );
    finishFailureFileSystem.failRemoveOnCall(1);
    const auto failedFinish = finishSetup
        ? DatabaseLifecycleService(finishFailureFileSystem)
            .finishInitialSetup(*finishSetup)
        : Status(std::unexpected(classmngr::engine::Error{
            ErrorCode::Internal,
            "test.setup-missing",
            std::nullopt
        }));
    passed &= expect(
        !failedFinish
            && hasBytes(
                finishFailureFileSystem,
                finishFailureDatabase,
                "finish failure new"
                )
            && hasBytes(
                finishFailureFileSystem,
                finishFailurePath,
                "finish failure original"
                ),
        "finish cleanup failure did not preserve both databases"
        );

    const std::string restoreFailureDatabase = pathToUtf8(
        root / "restore-failure.tps"
        );
    const std::string restoreFailurePath = pathToUtf8(
        root / "restore-failure.backup"
        );
    const std::string restoreFailureIncomplete = pathToUtf8(
        root / "restore-failure.incomplete"
        );
    FailureInjectingFileSystem restoreFailureFileSystem;
    passed &= expect(
        restoreFailureFileSystem.delegate().writeBytes(
            restoreFailureDatabase,
            "restore failure original"
            ).has_value(),
        "restore failure fixture could not be written"
        );
    const auto restoreSetup = DatabaseLifecycleService(
        restoreFailureFileSystem
        ).beginInitialSetup(
            restoreFailureDatabase,
            restoreFailurePath
            );
    passed &= expect(
        restoreSetup
            && restoreFailureFileSystem.delegate().writeBytes(
                restoreFailureDatabase,
                "restore failure new"
                ).has_value(),
        "restore failure setup could not be prepared"
        );
    restoreFailureFileSystem.failMoveOnCall(3);
    const auto failedRestore = restoreSetup
        ? DatabaseLifecycleService(restoreFailureFileSystem)
            .cancelInitialSetup(
                *restoreSetup,
                restoreFailureIncomplete
                )
        : Status(std::unexpected(classmngr::engine::Error{
            ErrorCode::Internal,
            "test.setup-missing",
            std::nullopt
        }));
    passed &= expect(
        !failedRestore
            && hasBytes(
                restoreFailureFileSystem,
                restoreFailureDatabase,
                "restore failure new"
                )
            && hasBytes(
                restoreFailureFileSystem,
                restoreFailurePath,
                "restore failure original"
                )
            && !restoreFailureFileSystem.delegate()
                .exists(restoreFailureIncomplete)
                .value_or(true),
        "restore failure did not keep the visible database recoverable"
        );

    const std::string cleanupFailureDatabase = pathToUtf8(
        root / "cleanup-failure.tps"
        );
    const std::string cleanupFailurePath = pathToUtf8(
        root / "cleanup-failure.backup"
        );
    const std::string cleanupFailureIncomplete = pathToUtf8(
        root / "cleanup-failure.incomplete"
        );
    FailureInjectingFileSystem cleanupFailureFileSystem;
    passed &= expect(
        cleanupFailureFileSystem.delegate().writeBytes(
            cleanupFailureDatabase,
            "cleanup failure original"
            ).has_value(),
        "cleanup failure fixture could not be written"
        );
    const auto cleanupSetup = DatabaseLifecycleService(
        cleanupFailureFileSystem
        ).beginInitialSetup(
            cleanupFailureDatabase,
            cleanupFailurePath
            );
    passed &= expect(
        cleanupSetup
            && cleanupFailureFileSystem.delegate().writeBytes(
                cleanupFailureDatabase,
                "cleanup failure new"
                ).has_value(),
        "cleanup failure setup could not be prepared"
        );
    cleanupFailureFileSystem.failRemoveOnCall(1);
    const auto failedCleanup = cleanupSetup
        ? DatabaseLifecycleService(cleanupFailureFileSystem)
            .cancelInitialSetup(
                *cleanupSetup,
                cleanupFailureIncomplete
                )
        : Status(std::unexpected(classmngr::engine::Error{
            ErrorCode::Internal,
            "test.setup-missing",
            std::nullopt
        }));
    passed &= expect(
        !failedCleanup
            && hasBytes(
                cleanupFailureFileSystem,
                cleanupFailureDatabase,
                "cleanup failure original"
                )
            && hasBytes(
                cleanupFailureFileSystem,
                cleanupFailureIncomplete,
                "cleanup failure new"
                )
            && !cleanupFailureFileSystem.delegate()
                .exists(cleanupFailurePath)
                .value_or(true),
        "cleanup failure did not restore the original database"
        );

    passed &= expect(
        fileSystem.removeTemporaryDirectory(*temporaryRoot).has_value(),
        "temporary lifecycle test directory could not be removed"
        );
    return passed ? 0 : 1;
}
