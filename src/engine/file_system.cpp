#include "classmngr/engine/file_system.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace classmngr::engine
{
namespace
{

namespace fs = std::filesystem;

constexpr std::size_t BufferSize = 64 * 1024;
constexpr std::string_view TemporaryFileSuffix =
    ".classmngr-file-system-temp-";
constexpr std::string_view BackupFileSuffix =
    ".classmngr-file-system-backup-";
constexpr std::string_view BackupDirectorySuffix =
    ".classmngr-file-system-directory-backup-";

std::atomic<std::uint64_t> TemporaryPathCounter = 0;

bool isBlank(
    std::string_view value
    ) noexcept
{
    if (value.empty())
    {
        return true;
    }

    for (const char character : value)
    {
        if (std::isspace(static_cast<unsigned char>(character)) == 0)
        {
            return false;
        }
    }

    return true;
}

bool containsNull(
    std::string_view value
    ) noexcept
{
    return value.find('\0') != std::string_view::npos;
}

std::optional<int> nativeCode(
    std::error_code error
    )
{
    if (!error)
    {
        return std::nullopt;
    }

    return error.value();
}

Error makeError(
    ErrorCode code,
    std::string_view token,
    std::error_code native = {}
    )
{
    return {
        code,
        std::string(token),
        nativeCode(native)
    };
}

Status failure(
    ErrorCode code,
    std::string_view token,
    std::error_code native = {}
    )
{
    return std::unexpected(makeError(code, token, native));
}

template<class T>
Result<T> failure(
    ErrorCode code,
    std::string_view token,
    std::error_code native = {}
    )
{
    return std::unexpected(makeError(code, token, native));
}

Status internalFailure()
{
    return failure(
        ErrorCode::Internal,
        FileSystemErrorToken::InternalError
        );
}

template<class T>
Result<T> internalFailure()
{
    return failure<T>(
        ErrorCode::Internal,
        FileSystemErrorToken::InternalError
        );
}

bool isMissing(
    std::error_code error
    ) noexcept
{
    return error == std::errc::no_such_file_or_directory
        || error == std::errc::not_a_directory;
}

fs::path pathFromUtf8(
    std::string_view value
    )
{
    return fs::u8path(value.begin(), value.end());
}

std::string pathToUtf8(
    const fs::path& path
    )
{
    // Keep the portable boundary's textual representation stable across
    // platforms. Qt's absolute-file-path APIs also expose forward slashes on
    // Windows, and callers may persist or compare the returned UTF-8 path.
    const std::u8string encoded = path.generic_u8string();
    std::string result(
        reinterpret_cast<const char*>(encoded.data()),
        encoded.size()
        );
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

Result<fs::path> normalizedPath(
    std::string_view utf8Path
    )
{
    if (isBlank(utf8Path) || containsNull(utf8Path))
    {
        return failure<fs::path>(
            ErrorCode::InvalidArgument,
            FileSystemErrorToken::InvalidPath
            );
    }

    try
    {
        const fs::path inputPath = pathFromUtf8(utf8Path);
        std::error_code error;
        const fs::path absolutePath = fs::absolute(inputPath, error);
        if (error)
        {
            return failure<fs::path>(
                ErrorCode::InvalidArgument,
                FileSystemErrorToken::InvalidPath,
                error
                );
        }

        return absolutePath.lexically_normal();
    }
    catch (const fs::filesystem_error& exception)
    {
        return failure<fs::path>(
            ErrorCode::InvalidArgument,
            FileSystemErrorToken::InvalidPath,
            exception.code()
            );
    }
    catch (const std::exception&)
    {
        return failure<fs::path>(
            ErrorCode::InvalidArgument,
            FileSystemErrorToken::InvalidPath
            );
    }
}

Status createDirectoriesAt(
    const fs::path& directoryPath
    )
{
    std::error_code error;
    const fs::file_status currentStatus = fs::status(directoryPath, error);
    if (error && !isMissing(error))
    {
        return failure(
            ErrorCode::Io,
            FileSystemErrorToken::DirectoryCreationFailed,
            error
            );
    }

    if (!error && fs::exists(currentStatus))
    {
        if (fs::is_directory(currentStatus))
        {
            return {};
        }

        return failure(
            ErrorCode::Io,
            FileSystemErrorToken::DirectoryCreationFailed
            );
    }

    fs::create_directories(directoryPath, error);
    if (error)
    {
        return failure(
            ErrorCode::Io,
            FileSystemErrorToken::DirectoryCreationFailed,
            error
            );
    }

    error.clear();
    const fs::file_status finalStatus = fs::status(directoryPath, error);
    if (error || !fs::is_directory(finalStatus))
    {
        return failure(
            ErrorCode::Io,
            FileSystemErrorToken::DirectoryCreationFailed,
            error
            );
    }

    return {};
}

Result<fs::file_status> regularSourceStatus(
    const fs::path& sourcePath
    )
{
    std::error_code error;
    const fs::file_status sourceStatus = fs::status(sourcePath, error);
    if (error)
    {
        if (isMissing(error))
        {
            return failure<fs::file_status>(
                ErrorCode::NotFound,
                FileSystemErrorToken::MissingSource,
                error
                );
        }

        return failure<fs::file_status>(
            ErrorCode::Io,
            FileSystemErrorToken::CopyFailed,
            error
            );
    }

    if (!fs::exists(sourceStatus))
    {
        return failure<fs::file_status>(
            ErrorCode::NotFound,
            FileSystemErrorToken::MissingSource
            );
    }

    if (!fs::is_regular_file(sourceStatus))
    {
        return failure<fs::file_status>(
            ErrorCode::InvalidArgument,
            FileSystemErrorToken::SourceNotRegular
            );
    }

    return sourceStatus;
}

Result<fs::path> uniqueSiblingPath(
    const fs::path& targetPath,
    std::string_view suffix,
    std::string_view failureToken
    )
{
    fs::path parentPath = targetPath.parent_path();
    if (parentPath.empty())
    {
        parentPath = fs::path(".");
    }

    const std::string targetName = pathToUtf8(targetPath.filename());
    const std::uint64_t counter = TemporaryPathCounter.fetch_add(
        1,
        std::memory_order_relaxed
        );
    const std::uint64_t clockValue = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count()
        );
    const std::uint64_t seed = clockValue ^ counter;

    for (std::uint64_t attempt = 0; attempt < 128; ++attempt)
    {
        const fs::path candidate = parentPath / pathFromUtf8(
            targetName
                + std::string(suffix)
                + std::to_string(seed + attempt)
            );
        std::error_code error;
        const bool candidateExists = fs::exists(candidate, error);
        if (error)
        {
            return failure<fs::path>(
                ErrorCode::Io,
                failureToken,
                error
                );
        }
        if (!candidateExists)
        {
            return candidate;
        }
    }

    return failure<fs::path>(
        ErrorCode::Io,
        failureToken
        );
}

class TemporaryFileGuard final
{
public:
    explicit TemporaryFileGuard(fs::path path)
        : m_path(std::move(path))
    {
    }

    ~TemporaryFileGuard()
    {
        if (!m_released)
        {
            std::error_code ignored;
            fs::remove(m_path, ignored);
        }
    }

    void release() noexcept
    {
        m_released = true;
    }

private:
    fs::path m_path;
    bool m_released = false;
};

bool writePayload(
    std::ofstream* destination,
    std::string_view bytes
    )
{
    constexpr std::size_t MaximumChunkSize = static_cast<std::size_t>(
        std::numeric_limits<std::streamsize>::max()
        );
    std::size_t offset = 0;
    while (offset < bytes.size())
    {
        const std::size_t chunkSize = std::min(
            MaximumChunkSize,
            bytes.size() - offset
            );
        destination->write(
            bytes.data() + offset,
            static_cast<std::streamsize>(chunkSize)
            );
        if (!*destination)
        {
            return false;
        }
        offset += chunkSize;
    }

    destination->flush();
    return static_cast<bool>(*destination);
}

bool copyStream(
    std::ifstream* source,
    std::ofstream* destination
    )
{
    std::array<char, BufferSize> buffer{};
    while (true)
    {
        source->read(
            buffer.data(),
            static_cast<std::streamsize>(buffer.size())
            );
        const std::streamsize bytesRead = source->gcount();
        if (bytesRead > 0)
        {
            destination->write(buffer.data(), bytesRead);
            if (!*destination)
            {
                return false;
            }
        }

        if (source->bad())
        {
            return false;
        }
        if (source->eof())
        {
            break;
        }
        if (source->fail())
        {
            return false;
        }
    }

    destination->flush();
    return static_cast<bool>(*destination);
}

Status closeOutput(
    std::ofstream* destination
    )
{
    destination->close();
    if (destination->fail())
    {
        return failure(
            ErrorCode::Io,
            FileSystemErrorToken::WriteFailed
            );
    }

    return {};
}

Status replaceFileAtomicallyAt(
    const fs::path& temporaryPath,
    const fs::path& destinationPath
    )
{
    if (temporaryPath == destinationPath)
    {
        return failure(
            ErrorCode::InvalidArgument,
            FileSystemErrorToken::InvalidPath
            );
    }

    const Result<fs::file_status> temporaryStatus = regularSourceStatus(
        temporaryPath
        );
    if (!temporaryStatus)
    {
        return std::unexpected(temporaryStatus.error());
    }

    std::error_code renameError;
    fs::rename(temporaryPath, destinationPath, renameError);
    if (!renameError)
    {
        return {};
    }

    std::error_code destinationStatusError;
    const fs::file_status destinationStatus = fs::status(
        destinationPath,
        destinationStatusError
        );
    if (destinationStatusError)
    {
        return failure(
            ErrorCode::Io,
            FileSystemErrorToken::AtomicReplacementFailed,
            destinationStatusError
            );
    }
    if (!fs::exists(destinationStatus)
        || !fs::is_regular_file(destinationStatus))
    {
        return failure(
            ErrorCode::Io,
            FileSystemErrorToken::AtomicReplacementFailed,
            renameError
            );
    }

    const Result<fs::path> backupPath = uniqueSiblingPath(
        destinationPath,
        BackupFileSuffix,
        FileSystemErrorToken::AtomicReplacementFailed
        );
    if (!backupPath)
    {
        return std::unexpected(backupPath.error());
    }

    std::error_code backupRenameError;
    fs::rename(destinationPath, *backupPath, backupRenameError);
    if (backupRenameError)
    {
        return failure(
            ErrorCode::Io,
            FileSystemErrorToken::AtomicReplacementFailed,
            backupRenameError
            );
    }

    std::error_code finalRenameError;
    fs::rename(temporaryPath, destinationPath, finalRenameError);
    if (finalRenameError)
    {
        std::error_code restoreError;
        fs::rename(*backupPath, destinationPath, restoreError);
        return failure(
            ErrorCode::Io,
            FileSystemErrorToken::AtomicReplacementFailed,
            finalRenameError
            );
    }

    std::error_code cleanupError;
    fs::remove(*backupPath, cleanupError);
    if (!cleanupError)
    {
        return {};
    }

    // If the private backup cannot be removed, restore the old destination so
    // a failed operation never leaves the new file as the visible result.
    std::error_code removeNewError;
    fs::remove(destinationPath, removeNewError);
    if (!removeNewError)
    {
        std::error_code restoreError;
        fs::rename(*backupPath, destinationPath, restoreError);
        if (!restoreError)
        {
            return failure(
                ErrorCode::Io,
                FileSystemErrorToken::AtomicReplacementFailed,
                cleanupError
                );
        }
    }

    return failure(
        ErrorCode::Io,
        FileSystemErrorToken::AtomicReplacementFailed,
        cleanupError
        );
}

Status replaceDirectoryAtomicallyAt(
    const fs::path& temporaryPath,
    const fs::path& destinationPath
    )
{
    if (temporaryPath == destinationPath)
    {
        return failure(
            ErrorCode::InvalidArgument,
            FileSystemErrorToken::InvalidPath
            );
    }

    std::error_code temporaryStatusError;
    const fs::file_status temporaryStatus = fs::status(
        temporaryPath,
        temporaryStatusError
        );
    if (temporaryStatusError)
    {
        if (isMissing(temporaryStatusError))
        {
            return failure(
                ErrorCode::NotFound,
                FileSystemErrorToken::MissingSource,
                temporaryStatusError
                );
        }

        return failure(
            ErrorCode::Io,
            FileSystemErrorToken::DirectoryReplacementFailed,
            temporaryStatusError
            );
    }
    if (!fs::exists(temporaryStatus)
        || !fs::is_directory(temporaryStatus))
    {
        return failure(
            ErrorCode::InvalidArgument,
            FileSystemErrorToken::DirectoryReplacementFailed
            );
    }

    std::error_code destinationStatusError;
    const fs::file_status destinationStatus = fs::status(
        destinationPath,
        destinationStatusError
        );
    if (destinationStatusError && !isMissing(destinationStatusError))
    {
        return failure(
            ErrorCode::Io,
            FileSystemErrorToken::DirectoryReplacementFailed,
            destinationStatusError
            );
    }

    const bool destinationExists = !destinationStatusError
        && fs::exists(destinationStatus);
    if (destinationExists && !fs::is_directory(destinationStatus))
    {
        return failure(
            ErrorCode::InvalidArgument,
            FileSystemErrorToken::DirectoryReplacementFailed
            );
    }

    if (!destinationExists)
    {
        std::error_code renameError;
        fs::rename(temporaryPath, destinationPath, renameError);
        if (!renameError)
        {
            return {};
        }

        return failure(
            ErrorCode::Io,
            FileSystemErrorToken::DirectoryReplacementFailed,
            renameError
            );
    }

    const Result<fs::path> backupPath = uniqueSiblingPath(
        destinationPath,
        BackupDirectorySuffix,
        FileSystemErrorToken::DirectoryReplacementFailed
        );
    if (!backupPath)
    {
        return std::unexpected(backupPath.error());
    }

    std::error_code backupRenameError;
    fs::rename(destinationPath, *backupPath, backupRenameError);
    if (backupRenameError)
    {
        return failure(
            ErrorCode::Io,
            FileSystemErrorToken::DirectoryReplacementFailed,
            backupRenameError
            );
    }

    std::error_code finalRenameError;
    fs::rename(temporaryPath, destinationPath, finalRenameError);
    if (finalRenameError)
    {
        std::error_code restoreError;
        fs::rename(*backupPath, destinationPath, restoreError);
        return failure(
            ErrorCode::Io,
            FileSystemErrorToken::DirectoryReplacementFailed,
            finalRenameError
            );
    }

    std::error_code cleanupError;
    fs::remove_all(*backupPath, cleanupError);
    if (!cleanupError)
    {
        return {};
    }

    std::error_code removeNewError;
    fs::remove_all(destinationPath, removeNewError);
    if (!removeNewError)
    {
        std::error_code restoreError;
        fs::rename(*backupPath, destinationPath, restoreError);
        if (!restoreError)
        {
            return failure(
                ErrorCode::Io,
                FileSystemErrorToken::DirectoryReplacementFailed,
                cleanupError
                );
        }
    }

    return failure(
        ErrorCode::Io,
        FileSystemErrorToken::DirectoryReplacementFailed,
        cleanupError
        );
}

} // namespace

Result<std::string> StandardFileSystem::normalizePath(
    std::string_view utf8Path
    ) const
{
    try
    {
        const Result<fs::path> normalized = normalizedPath(utf8Path);
        if (!normalized)
        {
            return std::unexpected(normalized.error());
        }
        return pathToUtf8(*normalized);
    }
    catch (const std::bad_alloc&)
    {
        return internalFailure<std::string>();
    }
    catch (const std::exception&)
    {
        return failure<std::string>(
            ErrorCode::InvalidArgument,
            FileSystemErrorToken::InvalidPath
            );
    }
    catch (...)
    {
        return internalFailure<std::string>();
    }
}

Result<bool> StandardFileSystem::exists(
    std::string_view utf8Path
    ) const
{
    try
    {
        const Result<fs::path> normalized = normalizedPath(utf8Path);
        if (!normalized)
        {
            return std::unexpected(normalized.error());
        }

        std::error_code error;
        const fs::file_status status = fs::status(*normalized, error);
        if (error)
        {
            if (isMissing(error))
            {
                return false;
            }

            return failure<bool>(
                ErrorCode::Io,
                FileSystemErrorToken::ExistenceCheckFailed,
                error
                );
        }

        return fs::exists(status);
    }
    catch (const std::bad_alloc&)
    {
        return internalFailure<bool>();
    }
    catch (const std::exception&)
    {
        return failure<bool>(
            ErrorCode::Io,
            FileSystemErrorToken::ExistenceCheckFailed
            );
    }
    catch (...)
    {
        return internalFailure<bool>();
    }
}

Status StandardFileSystem::createDirectories(
    std::string_view utf8DirectoryPath
    ) const
{
    try
    {
        const Result<fs::path> normalized = normalizedPath(
            utf8DirectoryPath
            );
        if (!normalized)
        {
            return std::unexpected(normalized.error());
        }

        return createDirectoriesAt(*normalized);
    }
    catch (const std::bad_alloc&)
    {
        return internalFailure();
    }
    catch (const std::exception&)
    {
        return failure(
            ErrorCode::Io,
            FileSystemErrorToken::DirectoryCreationFailed
            );
    }
    catch (...)
    {
        return internalFailure();
    }
}

Result<std::string> StandardFileSystem::readBytes(
    std::string_view utf8Path
    ) const
{
    try
    {
        const Result<fs::path> normalized = normalizedPath(utf8Path);
        if (!normalized)
        {
            return std::unexpected(normalized.error());
        }

        const Result<fs::file_status> sourceStatus = regularSourceStatus(
            *normalized
            );
        if (!sourceStatus)
        {
            return std::unexpected(sourceStatus.error());
        }

        std::ifstream source(*normalized, std::ios::binary);
        if (!source.is_open())
        {
            return failure<std::string>(
                ErrorCode::Io,
                FileSystemErrorToken::ReadFailed
                );
        }

        std::string bytes{
            std::istreambuf_iterator<char>(source),
            std::istreambuf_iterator<char>()
            };
        if (source.bad())
        {
            return failure<std::string>(
                ErrorCode::Io,
                FileSystemErrorToken::ReadFailed
                );
        }

        return bytes;
    }
    catch (const std::bad_alloc&)
    {
        return internalFailure<std::string>();
    }
    catch (const std::exception&)
    {
        return failure<std::string>(
            ErrorCode::Io,
            FileSystemErrorToken::ReadFailed
            );
    }
    catch (...)
    {
        return internalFailure<std::string>();
    }
}

Status StandardFileSystem::writeBytes(
    std::string_view utf8Path,
    std::string_view bytes,
    bool createParentDirectories
    ) const
{
    try
    {
        const Result<fs::path> normalized = normalizedPath(utf8Path);
        if (!normalized)
        {
            return std::unexpected(normalized.error());
        }

        if (createParentDirectories)
        {
            const fs::path parentPath = normalized->parent_path();
            if (!parentPath.empty())
            {
                const Status directories = createDirectoriesAt(parentPath);
                if (!directories)
                {
                    return std::unexpected(directories.error());
                }
            }
        }

        const Result<fs::path> temporaryPath = uniqueSiblingPath(
            *normalized,
            TemporaryFileSuffix,
            FileSystemErrorToken::WriteFailed
            );
        if (!temporaryPath)
        {
            return std::unexpected(temporaryPath.error());
        }

        TemporaryFileGuard temporaryFile(*temporaryPath);
        std::ofstream destination(
            *temporaryPath,
            std::ios::binary | std::ios::out | std::ios::trunc
            );
        if (!destination.is_open() || !writePayload(&destination, bytes))
        {
            return failure(
                ErrorCode::Io,
                FileSystemErrorToken::WriteFailed
                );
        }

        const Status closed = closeOutput(&destination);
        if (!closed)
        {
            return std::unexpected(closed.error());
        }

        const Status finalized = replaceFileAtomicallyAt(
            *temporaryPath,
            *normalized
            );
        if (!finalized)
        {
            return std::unexpected(finalized.error());
        }

        temporaryFile.release();
        return {};
    }
    catch (const std::bad_alloc&)
    {
        return internalFailure();
    }
    catch (const std::exception&)
    {
        return failure(
            ErrorCode::Io,
            FileSystemErrorToken::WriteFailed
            );
    }
    catch (...)
    {
        return internalFailure();
    }
}

Status StandardFileSystem::copyFile(
    std::string_view utf8SourcePath,
    std::string_view utf8DestinationPath,
    bool createParentDirectories
    ) const
{
    try
    {
        const Result<fs::path> sourcePath = normalizedPath(utf8SourcePath);
        if (!sourcePath)
        {
            return std::unexpected(sourcePath.error());
        }
        const Result<fs::path> destinationPath = normalizedPath(
            utf8DestinationPath
            );
        if (!destinationPath)
        {
            return std::unexpected(destinationPath.error());
        }

        const Result<fs::file_status> sourceStatus = regularSourceStatus(
            *sourcePath
            );
        if (!sourceStatus)
        {
            return std::unexpected(sourceStatus.error());
        }

        if (*sourcePath == *destinationPath)
        {
            return {};
        }

        if (createParentDirectories)
        {
            const fs::path parentPath = destinationPath->parent_path();
            if (!parentPath.empty())
            {
                const Status directories = createDirectoriesAt(parentPath);
                if (!directories)
                {
                    return std::unexpected(directories.error());
                }
            }
        }

        const Result<fs::path> temporaryPath = uniqueSiblingPath(
            *destinationPath,
            TemporaryFileSuffix,
            FileSystemErrorToken::CopyFailed
            );
        if (!temporaryPath)
        {
            return std::unexpected(temporaryPath.error());
        }

        TemporaryFileGuard temporaryFile(*temporaryPath);
        std::ifstream source(*sourcePath, std::ios::binary);
        std::ofstream destination(
            *temporaryPath,
            std::ios::binary | std::ios::out | std::ios::trunc
            );
        if (!source.is_open() || !destination.is_open())
        {
            if (!source.is_open())
            {
                std::error_code statusError;
                const fs::file_status currentStatus = fs::status(
                    *sourcePath,
                    statusError
                    );
                if (!statusError && !fs::exists(currentStatus))
                {
                    return failure(
                        ErrorCode::NotFound,
                        FileSystemErrorToken::MissingSource
                        );
                }
            }

            return failure(
                ErrorCode::Io,
                FileSystemErrorToken::CopyFailed
                );
        }

        if (!copyStream(&source, &destination))
        {
            return failure(
                ErrorCode::Io,
                FileSystemErrorToken::CopyFailed
                );
        }

        const Status closed = closeOutput(&destination);
        if (!closed)
        {
            return failure(
                ErrorCode::Io,
                FileSystemErrorToken::CopyFailed
                );
        }

        const Status finalized = replaceFileAtomicallyAt(
            *temporaryPath,
            *destinationPath
            );
        if (!finalized)
        {
            return std::unexpected(finalized.error());
        }

        temporaryFile.release();
        return {};
    }
    catch (const std::bad_alloc&)
    {
        return internalFailure();
    }
    catch (const std::exception&)
    {
        return failure(
            ErrorCode::Io,
            FileSystemErrorToken::CopyFailed
            );
    }
    catch (...)
    {
        return internalFailure();
    }
}

Status StandardFileSystem::replaceFileAtomically(
    std::string_view utf8TemporaryPath,
    std::string_view utf8DestinationPath
    ) const
{
    try
    {
        const Result<fs::path> temporaryPath = normalizedPath(
            utf8TemporaryPath
            );
        if (!temporaryPath)
        {
            return std::unexpected(temporaryPath.error());
        }
        const Result<fs::path> destinationPath = normalizedPath(
            utf8DestinationPath
            );
        if (!destinationPath)
        {
            return std::unexpected(destinationPath.error());
        }

        return replaceFileAtomicallyAt(*temporaryPath, *destinationPath);
    }
    catch (const std::bad_alloc&)
    {
        return internalFailure();
    }
    catch (const std::exception&)
    {
        return failure(
            ErrorCode::Io,
            FileSystemErrorToken::AtomicReplacementFailed
            );
    }
    catch (...)
    {
        return internalFailure();
    }
}

Status StandardFileSystem::replaceDirectoryAtomically(
    std::string_view utf8TemporaryDirectoryPath,
    std::string_view utf8DestinationDirectoryPath
    ) const
{
    try
    {
        const Result<fs::path> temporaryPath = normalizedPath(
            utf8TemporaryDirectoryPath
            );
        if (!temporaryPath)
        {
            return std::unexpected(temporaryPath.error());
        }
        const Result<fs::path> destinationPath = normalizedPath(
            utf8DestinationDirectoryPath
            );
        if (!destinationPath)
        {
            return std::unexpected(destinationPath.error());
        }

        return replaceDirectoryAtomicallyAt(
            *temporaryPath,
            *destinationPath
            );
    }
    catch (const std::bad_alloc&)
    {
        return internalFailure();
    }
    catch (const std::exception&)
    {
        return failure(
            ErrorCode::Io,
            FileSystemErrorToken::DirectoryReplacementFailed
            );
    }
    catch (...)
    {
        return internalFailure();
    }
}

Status StandardFileSystem::removeFile(
    std::string_view utf8Path
    ) const
{
    try
    {
        const Result<fs::path> normalized = normalizedPath(utf8Path);
        if (!normalized)
        {
            return std::unexpected(normalized.error());
        }

        std::error_code statusError;
        const fs::file_status status = fs::status(*normalized, statusError);
        if (statusError)
        {
            if (isMissing(statusError))
            {
                return {};
            }

            return failure(
                ErrorCode::Io,
                FileSystemErrorToken::RemoveFailed,
                statusError
                );
        }
        if (!fs::exists(status))
        {
            return {};
        }
        if (!fs::is_regular_file(status))
        {
            return failure(
                ErrorCode::InvalidArgument,
                FileSystemErrorToken::RemoveFailed
                );
        }

        std::error_code removeError;
        fs::remove(*normalized, removeError);
        if (removeError)
        {
            return failure(
                ErrorCode::Io,
                FileSystemErrorToken::RemoveFailed,
                removeError
                );
        }

        return {};
    }
    catch (const std::bad_alloc&)
    {
        return internalFailure();
    }
    catch (const std::exception&)
    {
        return failure(
            ErrorCode::Io,
            FileSystemErrorToken::RemoveFailed
            );
    }
    catch (...)
    {
        return internalFailure();
    }
}

Result<std::string> StandardFileSystem::createTemporaryDirectory(
    std::string_view utf8ParentDirectory
    ) const
{
    try
    {
        const Result<fs::path> parentPath = normalizedPath(
            utf8ParentDirectory
            );
        if (!parentPath)
        {
            return std::unexpected(parentPath.error());
        }

        std::error_code parentStatusError;
        const fs::file_status parentStatus = fs::status(
            *parentPath,
            parentStatusError
            );
        if (parentStatusError
            || !fs::exists(parentStatus)
            || !fs::is_directory(parentStatus))
        {
            return failure<std::string>(
                ErrorCode::Io,
                FileSystemErrorToken::TemporaryDirectoryFailed,
                parentStatusError
                );
        }

        const std::uint64_t counter = TemporaryPathCounter.fetch_add(
            1,
            std::memory_order_relaxed
            );
        const std::uint64_t clockValue = static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()
            );
        const std::uint64_t seed = clockValue ^ counter;
        for (std::uint64_t attempt = 0; attempt < 128; ++attempt)
        {
            const fs::path candidate = *parentPath / pathFromUtf8(
                ".classmngr-temporary-" + std::to_string(seed + attempt)
                );
            std::error_code createError;
            if (fs::create_directory(candidate, createError))
            {
                return pathToUtf8(candidate);
            }
            if (createError != std::errc::file_exists)
            {
                return failure<std::string>(
                    ErrorCode::Io,
                    FileSystemErrorToken::TemporaryDirectoryFailed,
                    createError
                    );
            }
        }

        return failure<std::string>(
            ErrorCode::Io,
            FileSystemErrorToken::TemporaryDirectoryFailed
            );
    }
    catch (const std::bad_alloc&)
    {
        return internalFailure<std::string>();
    }
    catch (const std::exception&)
    {
        return failure<std::string>(
            ErrorCode::Io,
            FileSystemErrorToken::TemporaryDirectoryFailed
            );
    }
    catch (...)
    {
        return internalFailure<std::string>();
    }
}

Status StandardFileSystem::removeTemporaryDirectory(
    std::string_view utf8DirectoryPath
    ) const
{
    try
    {
        const Result<fs::path> directoryPath = normalizedPath(
            utf8DirectoryPath
            );
        if (!directoryPath)
        {
            return std::unexpected(directoryPath.error());
        }

        std::error_code statusError;
        const fs::file_status status = fs::status(
            *directoryPath,
            statusError
            );
        if (statusError)
        {
            if (isMissing(statusError))
            {
                return {};
            }
            return failure(
                ErrorCode::Io,
                FileSystemErrorToken::TemporaryDirectoryCleanupFailed,
                statusError
                );
        }
        if (!fs::exists(status))
        {
            return {};
        }
        if (!fs::is_directory(status))
        {
            return failure(
                ErrorCode::InvalidArgument,
                FileSystemErrorToken::TemporaryDirectoryCleanupFailed
                );
        }

        std::error_code removeError;
        fs::remove_all(*directoryPath, removeError);
        if (removeError)
        {
            return failure(
                ErrorCode::Io,
                FileSystemErrorToken::TemporaryDirectoryCleanupFailed,
                removeError
                );
        }

        return {};
    }
    catch (const std::bad_alloc&)
    {
        return internalFailure();
    }
    catch (const std::exception&)
    {
        return failure(
            ErrorCode::Io,
            FileSystemErrorToken::TemporaryDirectoryCleanupFailed
            );
    }
    catch (...)
    {
        return internalFailure();
    }
}

} // namespace classmngr::engine
