#include "classmngr/engine/zip_archive_writer.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace classmngr::engine::ZipArchiveWriter
{
namespace
{

namespace fs = std::filesystem;

constexpr std::uint32_t LocalFileHeaderSignature = 0x04034b50;
constexpr std::uint32_t CentralDirectoryHeaderSignature = 0x02014b50;
constexpr std::uint32_t EndOfCentralDirectorySignature = 0x06054b50;
constexpr std::uint16_t ZipVersion20 = 20;
constexpr std::uint16_t Utf8FileNameFlag = 0x0800;
constexpr std::size_t BufferSize = 64 * 1024;
constexpr std::uint32_t MaximumZip32Value =
    std::numeric_limits<std::uint32_t>::max();

constexpr std::string_view EmptyEntryListToken = "empty-entry-list";
constexpr std::string_view EntryCountLimitToken = "entry-count-limit";
constexpr std::string_view InvalidEntryNameToken = "invalid-entry-name";
constexpr std::string_view DuplicateEntryNameToken = "duplicate-entry-name";
constexpr std::string_view SourceOpenFailedToken = "source-open-failed";
constexpr std::string_view SourceTooLargeToken = "source-too-large";
constexpr std::string_view EntryNameTooLongToken = "entry-name-too-long";
constexpr std::string_view SourceReadFailedToken = "source-read-failed";
constexpr std::string_view ArchiveOpenFailedToken = "archive-open-failed";
constexpr std::string_view ArchiveSizeLimitToken = "archive-size-limit";
constexpr std::string_view ArchiveEntryWriteFailedToken =
    "archive-entry-write-failed";
constexpr std::string_view ArchiveDirectoryWriteFailedToken =
    "archive-directory-write-failed";
constexpr std::string_view ArchiveFinalizeFailedToken =
    "archive-finalize-failed";
constexpr std::string_view InternalErrorToken = "internal-error";

Status failure(
    ErrorCode code,
    std::string_view token
    )
{
    return std::unexpected(Error{
        code,
        std::string(token),
        std::nullopt
    });
}

fs::path pathFromUtf8(
    std::string_view value
    )
{
    std::u8string encoded;
    encoded.reserve(value.size());
    for (const char character : value)
    {
        encoded.push_back(
            static_cast<char8_t>(
                static_cast<unsigned char>(character)
                )
            );
    }
    return fs::path(encoded);
}

std::string pathFileNameUtf8(
    const fs::path& path
    )
{
    const std::u8string encoded = path.filename().u8string();
    std::string result;
    result.reserve(encoded.size());
    for (const char8_t character : encoded)
    {
        result.push_back(static_cast<char>(character));
    }
    return result;
}

bool isAsciiWhitespace(
    char character
    ) noexcept
{
    switch (character)
    {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
    case '\f':
    case '\v':
        return true;
    default:
        return false;
    }
}

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
        if (!isAsciiWhitespace(character))
        {
            return false;
        }
    }

    return true;
}

bool validArchiveName(
    std::string_view archiveName
    ) noexcept
{
    if (isBlank(archiveName)
        || archiveName.front() == '/'
        || (archiveName.size() >= 2
            && archiveName[1] == ':'
            && ((archiveName[0] >= 'A' && archiveName[0] <= 'Z')
                || (archiveName[0] >= 'a' && archiveName[0] <= 'z')))
        || archiveName.find('\\') != std::string_view::npos)
    {
        return false;
    }

    std::size_t componentStart = 0;
    while (componentStart <= archiveName.size())
    {
        const std::size_t separator = archiveName.find(
            '/',
            componentStart
            );
        const std::size_t componentLength = separator == std::string_view::npos
            ? archiveName.size() - componentStart
            : separator - componentStart;
        const std::string_view component = archiveName.substr(
            componentStart,
            componentLength
            );
        if (component.empty()
            || component == "."
            || component == "..")
        {
            return false;
        }

        if (separator == std::string_view::npos)
        {
            break;
        }
        componentStart = separator + 1;
    }

    return true;
}

std::uint32_t crc32Update(
    std::uint32_t crc,
    const char* data,
    std::size_t size
    ) noexcept
{
    crc = ~crc;
    for (std::size_t index = 0; index < size; ++index)
    {
        crc ^= static_cast<unsigned char>(data[index]);
        for (int bit = 0; bit < 8; ++bit)
        {
            crc = (crc >> 1)
                ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

void appendLe16(
    std::string* data,
    std::uint16_t value
    )
{
    data->push_back(static_cast<char>(value & 0xffu));
    data->push_back(static_cast<char>((value >> 8) & 0xffu));
}

void appendLe32(
    std::string* data,
    std::uint32_t value
    )
{
    appendLe16(data, static_cast<std::uint16_t>(value & 0xffffu));
    appendLe16(data, static_cast<std::uint16_t>((value >> 16) & 0xffffu));
}

void dosDateTime(
    const fs::path& sourcePath,
    std::uint16_t* time,
    std::uint16_t* date
    ) noexcept
{
    using FileClock = fs::file_time_type::clock;

    std::chrono::system_clock::time_point systemTime =
        std::chrono::system_clock::now();
    std::error_code error;
    const fs::file_time_type fileTime = fs::last_write_time(
        sourcePath,
        error
        );
    if (!error)
    {
        // file_time_type has no portable epoch conversion.  Relating it to
        // the current values of both clocks is the standard-library bridge
        // used here; an unavailable conversion falls back to current time.
        const auto fileClockDelta = fileTime - FileClock::now();
        const auto systemClockDelta = std::chrono::duration_cast<
            std::chrono::system_clock::duration
            >(fileClockDelta);
        systemTime = std::chrono::system_clock::now() + systemClockDelta;
    }

    std::time_t timeValue = std::chrono::system_clock::to_time_t(systemTime);
    std::tm localTime{};
    const std::tm* converted = std::localtime(&timeValue);
    if (converted)
    {
        localTime = *converted;
    }
    else
    {
        timeValue = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now()
            );
        converted = std::localtime(&timeValue);
        if (converted)
        {
            localTime = *converted;
        }
        else
        {
            localTime = {};
            localTime.tm_year = 80;
            localTime.tm_mon = 0;
            localTime.tm_mday = 1;
        }
    }

    const int year = std::clamp(localTime.tm_year + 1900, 1980, 2107);
    const int month = std::clamp(localTime.tm_mon + 1, 1, 12);
    const int day = std::clamp(localTime.tm_mday, 1, 31);
    const int hour = std::clamp(localTime.tm_hour, 0, 23);
    const int minute = std::clamp(localTime.tm_min, 0, 59);
    const int second = std::clamp(localTime.tm_sec, 0, 59);

    *time = static_cast<std::uint16_t>(
        (hour << 11)
        | (minute << 5)
        | (second / 2)
        );
    *date = static_cast<std::uint16_t>(
        ((year - 1980) << 9)
        | (month << 5)
        | day
        );
}

struct PreparedEntry
{
    fs::path sourcePath;
    std::string archiveName;
    std::uint32_t crc = 0;
    std::uint32_t size = 0;
    std::uint32_t localHeaderOffset = 0;
    std::uint16_t modifiedTime = 0;
    std::uint16_t modifiedDate = 0;
};

template<class Callback>
bool readSourceChunks(
    std::ifstream* source,
    std::uint64_t expectedSize,
    Callback&& callback
    )
{
    std::array<char, BufferSize> buffer{};
    std::uint64_t totalSize = 0;
    while (source->good())
    {
        source->read(
            buffer.data(),
            static_cast<std::streamsize>(buffer.size())
            );
        const std::streamsize bytesRead = source->gcount();
        if (bytesRead > 0)
        {
            totalSize += static_cast<std::uint64_t>(bytesRead);
            if (totalSize > expectedSize
                || totalSize > MaximumZip32Value)
            {
                return false;
            }
            if (!callback(
                    buffer.data(),
                    static_cast<std::size_t>(bytesRead)
                    ))
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

    return totalSize == expectedSize;
}

std::string localHeader(
    const PreparedEntry& entry
    )
{
    std::string header;
    header.reserve(30 + entry.archiveName.size());
    appendLe32(&header, LocalFileHeaderSignature);
    appendLe16(&header, ZipVersion20);
    appendLe16(&header, Utf8FileNameFlag);
    appendLe16(&header, 0);
    appendLe16(&header, entry.modifiedTime);
    appendLe16(&header, entry.modifiedDate);
    appendLe32(&header, entry.crc);
    appendLe32(&header, entry.size);
    appendLe32(&header, entry.size);
    appendLe16(
        &header,
        static_cast<std::uint16_t>(entry.archiveName.size())
        );
    appendLe16(&header, 0);
    header.append(entry.archiveName);
    return header;
}

std::string centralDirectoryHeader(
    const PreparedEntry& entry
    )
{
    std::string header;
    header.reserve(46 + entry.archiveName.size());
    appendLe32(&header, CentralDirectoryHeaderSignature);
    appendLe16(&header, ZipVersion20);
    appendLe16(&header, ZipVersion20);
    appendLe16(&header, Utf8FileNameFlag);
    appendLe16(&header, 0);
    appendLe16(&header, entry.modifiedTime);
    appendLe16(&header, entry.modifiedDate);
    appendLe32(&header, entry.crc);
    appendLe32(&header, entry.size);
    appendLe32(&header, entry.size);
    appendLe16(
        &header,
        static_cast<std::uint16_t>(entry.archiveName.size())
        );
    appendLe16(&header, 0);
    appendLe16(&header, 0);
    appendLe16(&header, 0);
    appendLe16(&header, 0);
    appendLe32(&header, 0);
    appendLe32(&header, entry.localHeaderOffset);
    header.append(entry.archiveName);
    return header;
}

bool writeBytes(
    std::ofstream* destination,
    const std::string& data
    )
{
    destination->write(
        data.data(),
        static_cast<std::streamsize>(data.size())
        );
    return static_cast<bool>(*destination);
}

bool canAdvance(
    std::uint64_t position,
    std::uint64_t bytes
    ) noexcept
{
    return position <= MaximumZip32Value
        && bytes <= MaximumZip32Value - position;
}

std::atomic<std::uint64_t> TemporaryPathCounter = 0;

bool makeUniqueSiblingPath(
    const fs::path& targetPath,
    std::string_view suffix,
    fs::path* result
    )
{
    if (!result)
    {
        return false;
    }

    fs::path parentPath = targetPath.parent_path();
    if (parentPath.empty())
    {
        parentPath = fs::path(".");
    }

    const std::string targetName = pathFileNameUtf8(targetPath);
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
        const bool exists = fs::exists(candidate, error);
        if (error)
        {
            return false;
        }
        if (!exists)
        {
            *result = candidate;
            return true;
        }
    }

    return false;
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

bool replaceArchive(
    const fs::path& temporaryPath,
    const fs::path& targetPath
    )
{
    std::error_code error;
    fs::rename(temporaryPath, targetPath, error);
    if (!error)
    {
        return true;
    }

    std::error_code existsError;
    const bool targetExists = fs::exists(targetPath, existsError);
    if (existsError || !targetExists)
    {
        return false;
    }

    std::error_code statusError;
    if (!fs::is_regular_file(fs::status(targetPath, statusError))
        || statusError)
    {
        return false;
    }

    fs::path backupPath;
    if (!makeUniqueSiblingPath(
            targetPath,
            ".classmngr-zip-backup-",
            &backupPath
            ))
    {
        return false;
    }

    error.clear();
    fs::rename(targetPath, backupPath, error);
    if (error)
    {
        return false;
    }

    error.clear();
    fs::rename(temporaryPath, targetPath, error);
    if (error)
    {
        std::error_code restoreError;
        fs::rename(backupPath, targetPath, restoreError);
        return false;
    }

    std::error_code cleanupError;
    fs::remove(backupPath, cleanupError);
    if (!cleanupError)
    {
        return true;
    }

    // Keep the previous archive if the standard-library replacement fallback
    // cannot remove its private backup.  Both operations are best-effort;
    // the explicit paths keep cleanup narrow if the filesystem is unwritable.
    std::error_code removeNewError;
    fs::remove(targetPath, removeNewError);
    std::error_code restoreError;
    fs::rename(backupPath, targetPath, restoreError);
    return false;
}

Status writeArchiveImpl(
    const std::string& archivePath,
    const std::vector<Entry>& entries
    )
{
    if (entries.empty())
    {
        return failure(ErrorCode::InvalidArgument, EmptyEntryListToken);
    }
    if (entries.size() > std::numeric_limits<std::uint16_t>::max())
    {
        return failure(ErrorCode::InvalidArgument, EntryCountLimitToken);
    }

    std::vector<PreparedEntry> preparedEntries;
    preparedEntries.reserve(entries.size());
    std::set<std::string> archiveNames;
    for (const Entry& entry : entries)
    {
        if (archiveNames.contains(entry.archiveName))
        {
            return failure(
                ErrorCode::InvalidArgument,
                DuplicateEntryNameToken
                );
        }

        PreparedEntry prepared;
        if (!validArchiveName(entry.archiveName))
        {
            return failure(
                ErrorCode::InvalidArgument,
                InvalidEntryNameToken
                );
        }
        if (entry.archiveName.size()
            > std::numeric_limits<std::uint16_t>::max())
        {
            return failure(
                ErrorCode::NumericOverflow,
                EntryNameTooLongToken
                );
        }

        const fs::path sourcePath = pathFromUtf8(entry.sourcePath);
        std::error_code sourceStatusError;
        const fs::file_status sourceStatus = fs::status(
            sourcePath,
            sourceStatusError
            );
        if (sourceStatusError || !fs::is_regular_file(sourceStatus))
        {
            return failure(ErrorCode::Io, SourceOpenFailedToken);
        }
        std::error_code sourceSizeError;
        const std::uintmax_t sourceSize = fs::file_size(
            sourcePath,
            sourceSizeError
            );
        if (sourceSizeError)
        {
            return failure(ErrorCode::Io, SourceOpenFailedToken);
        }
        if (sourceSize > MaximumZip32Value)
        {
            return failure(ErrorCode::NumericOverflow, SourceTooLargeToken);
        }

        std::ifstream source(sourcePath, std::ios::binary);
        if (!source.is_open())
        {
            return failure(ErrorCode::Io, SourceOpenFailedToken);
        }
        std::uint32_t crc = 0;
        if (!readSourceChunks(
                &source,
                static_cast<std::uint64_t>(sourceSize),
                [&crc](const char* data, std::size_t size)
                {
                    crc = crc32Update(crc, data, size);
                    return true;
                }
                ))
        {
            return failure(ErrorCode::Io, SourceReadFailedToken);
        }

        prepared.sourcePath = sourcePath;
        prepared.archiveName = entry.archiveName;
        prepared.crc = crc;
        prepared.size = static_cast<std::uint32_t>(sourceSize);
        dosDateTime(
            sourcePath,
            &prepared.modifiedTime,
            &prepared.modifiedDate
            );
        preparedEntries.push_back(std::move(prepared));
        archiveNames.insert(entry.archiveName);
    }

    if (archivePath.empty())
    {
        return failure(ErrorCode::Io, ArchiveOpenFailedToken);
    }

    const fs::path targetPath = pathFromUtf8(archivePath);
    fs::path temporaryPath;
    if (!makeUniqueSiblingPath(
            targetPath,
            ".classmngr-zip-temp-",
            &temporaryPath
            ))
    {
        return failure(ErrorCode::Io, ArchiveOpenFailedToken);
    }

    TemporaryFileGuard temporaryFile(temporaryPath);
    std::ofstream archive(
        temporaryPath,
        std::ios::binary | std::ios::out | std::ios::trunc
        );
    if (!archive.is_open())
    {
        return failure(ErrorCode::Io, ArchiveOpenFailedToken);
    }

    std::uint64_t position = 0;
    for (PreparedEntry& entry : preparedEntries)
    {
        const std::string header = localHeader(entry);
        if (!canAdvance(position, header.size())
            || !canAdvance(
                position + static_cast<std::uint64_t>(header.size()),
                entry.size
                ))
        {
            return failure(ErrorCode::NumericOverflow, ArchiveSizeLimitToken);
        }

        entry.localHeaderOffset = static_cast<std::uint32_t>(position);
        if (!writeBytes(&archive, header))
        {
            return failure(ErrorCode::Io, ArchiveEntryWriteFailedToken);
        }
        position += static_cast<std::uint64_t>(header.size());

        std::ifstream source(entry.sourcePath, std::ios::binary);
        if (!source.is_open())
        {
            return failure(ErrorCode::Io, ArchiveEntryWriteFailedToken);
        }

        std::uint32_t copiedCrc = 0;
        if (!readSourceChunks(
                &source,
                entry.size,
                [&archive, &copiedCrc](const char* data, std::size_t size)
                {
                    archive.write(
                        data,
                        static_cast<std::streamsize>(size)
                        );
                    if (!archive)
                    {
                        return false;
                    }
                    copiedCrc = crc32Update(copiedCrc, data, size);
                    return true;
                }
                )
            || copiedCrc != entry.crc)
        {
            return failure(ErrorCode::Io, ArchiveEntryWriteFailedToken);
        }
        position += entry.size;
    }

    if (!canAdvance(position, 0))
    {
        return failure(ErrorCode::NumericOverflow, ArchiveSizeLimitToken);
    }
    const std::uint32_t centralDirectoryOffset =
        static_cast<std::uint32_t>(position);
    for (const PreparedEntry& entry : preparedEntries)
    {
        const std::string header = centralDirectoryHeader(entry);
        if (!canAdvance(position, header.size()))
        {
            return failure(ErrorCode::NumericOverflow, ArchiveSizeLimitToken);
        }
        if (!writeBytes(&archive, header))
        {
            return failure(
                ErrorCode::Io,
                ArchiveDirectoryWriteFailedToken
                );
        }
        position += static_cast<std::uint64_t>(header.size());
    }

    const std::uint64_t centralDirectorySize =
        position - centralDirectoryOffset;
    if (centralDirectorySize > MaximumZip32Value)
    {
        return failure(ErrorCode::NumericOverflow, ArchiveSizeLimitToken);
    }

    std::string endRecord;
    endRecord.reserve(22);
    appendLe32(&endRecord, EndOfCentralDirectorySignature);
    appendLe16(&endRecord, 0);
    appendLe16(&endRecord, 0);
    appendLe16(
        &endRecord,
        static_cast<std::uint16_t>(preparedEntries.size())
        );
    appendLe16(
        &endRecord,
        static_cast<std::uint16_t>(preparedEntries.size())
        );
    appendLe32(
        &endRecord,
        static_cast<std::uint32_t>(centralDirectorySize)
        );
    appendLe32(&endRecord, centralDirectoryOffset);
    appendLe16(&endRecord, 0);

    if (!canAdvance(position, endRecord.size())
        || !writeBytes(&archive, endRecord))
    {
        return failure(ErrorCode::Io, ArchiveFinalizeFailedToken);
    }

    archive.flush();
    if (!archive)
    {
        return failure(ErrorCode::Io, ArchiveFinalizeFailedToken);
    }
    archive.close();
    if (archive.fail())
    {
        return failure(ErrorCode::Io, ArchiveFinalizeFailedToken);
    }

    if (!replaceArchive(temporaryPath, targetPath))
    {
        return failure(ErrorCode::Io, ArchiveFinalizeFailedToken);
    }

    temporaryFile.release();
    return {};
}

} // namespace

Status writeArchive(
    const std::string& archivePath,
    const std::vector<Entry>& entries
    )
{
    try
    {
        return writeArchiveImpl(archivePath, entries);
    }
    catch (const std::bad_alloc&)
    {
        return failure(ErrorCode::Internal, InternalErrorToken);
    }
    catch (const std::exception&)
    {
        return failure(ErrorCode::Internal, InternalErrorToken);
    }
    catch (...)
    {
        return failure(ErrorCode::Internal, InternalErrorToken);
    }
}

} // namespace classmngr::engine::ZipArchiveWriter
