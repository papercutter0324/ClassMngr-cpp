#pragma once

#include "classmngr/engine/platform_services.h"
#include "classmngr/engine/result.h"

#include <string>
#include <string_view>

namespace classmngr::engine
{

// File-system errors use stable tokens so presentation adapters can localize
// them without making the portable engine depend on a UI framework.
namespace FileSystemErrorToken
{
inline constexpr std::string_view InvalidPath =
    "file-system.invalid-path";
inline constexpr std::string_view MissingSource =
    "file-system.missing-source";
inline constexpr std::string_view SourceNotRegular =
    "file-system.source-not-regular";
inline constexpr std::string_view ExistenceCheckFailed =
    "file-system.existence-check-failed";
inline constexpr std::string_view DirectoryCreationFailed =
    "file-system.directory-creation-failed";
inline constexpr std::string_view ReadFailed =
    "file-system.read-failed";
inline constexpr std::string_view WriteFailed =
    "file-system.write-failed";
inline constexpr std::string_view CopyFailed =
    "file-system.copy-failed";
inline constexpr std::string_view AtomicReplacementFailed =
    "file-system.atomic-replacement-failed";
inline constexpr std::string_view DirectoryReplacementFailed =
    "file-system.directory-replacement-failed";
inline constexpr std::string_view RemoveFailed =
    "file-system.remove-failed";
inline constexpr std::string_view TemporaryDirectoryFailed =
    "file-system.temporary-directory-failed";
inline constexpr std::string_view TemporaryDirectoryCleanupFailed =
    "file-system.temporary-directory-cleanup-failed";
inline constexpr std::string_view InternalError =
    "file-system.internal-error";
} // namespace FileSystemErrorToken

// All path arguments and returned paths are UTF-8. Byte payloads are held in
// std::string so arbitrary bytes, including embedded nulls, can be preserved.
// copyFile() writes through a sibling temporary file and finalizes it with
// replaceFileAtomically(), so a failed copy does not leave a partial target.
class FileSystem
{
public:
    virtual ~FileSystem() = default;

    [[nodiscard]] virtual Result<std::string> normalizePath(
        std::string_view utf8Path
        ) const = 0;

    [[nodiscard]] virtual Result<bool> exists(
        std::string_view utf8Path
        ) const = 0;

    [[nodiscard]] virtual Status createDirectories(
        std::string_view utf8DirectoryPath
        ) const = 0;

    [[nodiscard]] virtual Result<std::string> readBytes(
        std::string_view utf8Path
        ) const = 0;

    [[nodiscard]] virtual Status writeBytes(
        std::string_view utf8Path,
        std::string_view bytes,
        bool createParentDirectories = false
        ) const = 0;

    [[nodiscard]] virtual Status copyFile(
        std::string_view utf8SourcePath,
        std::string_view utf8DestinationPath,
        bool createParentDirectories = false
        ) const = 0;

    // The temporary path must be on the same filesystem as the destination.
    // The implementation uses rename where possible and a deterministic
    // backup/restore fallback where the standard library cannot replace an
    // existing file directly. The invariant is no partial destination; a
    // fallback replacement may briefly leave the destination absent.
    [[nodiscard]] virtual Status replaceFileAtomically(
        std::string_view utf8TemporaryPath,
        std::string_view utf8DestinationPath
        ) const = 0;

    // Replaces a staged sibling directory while preserving the existing
    // destination if the replacement or cleanup fails.
    [[nodiscard]] virtual Status replaceDirectoryAtomically(
        std::string_view utf8TemporaryDirectoryPath,
        std::string_view utf8DestinationDirectoryPath
        ) const = 0;

    [[nodiscard]] virtual Status removeFile(
        std::string_view utf8Path
        ) const = 0;

    [[nodiscard]] virtual Result<std::string> createTemporaryDirectory(
        std::string_view utf8ParentDirectory
        ) const = 0;

    [[nodiscard]] virtual Status removeTemporaryDirectory(
        std::string_view utf8DirectoryPath
        ) const = 0;
};

class StandardFileSystem final : public FileSystem
{
public:
    // The default keeps existing callers on the system clock.  Tests and
    // workflows that need reproducible temporary names can supply a clock.
    StandardFileSystem();
    explicit StandardFileSystem(const Clock& clock);

    [[nodiscard]] Result<std::string> normalizePath(
        std::string_view utf8Path
        ) const override;

    [[nodiscard]] Result<bool> exists(
        std::string_view utf8Path
        ) const override;

    [[nodiscard]] Status createDirectories(
        std::string_view utf8DirectoryPath
        ) const override;

    [[nodiscard]] Result<std::string> readBytes(
        std::string_view utf8Path
        ) const override;

    [[nodiscard]] Status writeBytes(
        std::string_view utf8Path,
        std::string_view bytes,
        bool createParentDirectories = false
        ) const override;

    [[nodiscard]] Status copyFile(
        std::string_view utf8SourcePath,
        std::string_view utf8DestinationPath,
        bool createParentDirectories = false
        ) const override;

    [[nodiscard]] Status replaceFileAtomically(
        std::string_view utf8TemporaryPath,
        std::string_view utf8DestinationPath
        ) const override;

    [[nodiscard]] Status replaceDirectoryAtomically(
        std::string_view utf8TemporaryDirectoryPath,
        std::string_view utf8DestinationDirectoryPath
        ) const override;

    [[nodiscard]] Status removeFile(
        std::string_view utf8Path
        ) const override;

    [[nodiscard]] Result<std::string> createTemporaryDirectory(
        std::string_view utf8ParentDirectory
        ) const override;

    [[nodiscard]] Status removeTemporaryDirectory(
        std::string_view utf8DirectoryPath
        ) const override;

private:
    [[nodiscard]] const Clock& clock() const noexcept;

    SystemClock m_systemClock;
    const Clock* m_clock = nullptr;
};

} // namespace classmngr::engine
