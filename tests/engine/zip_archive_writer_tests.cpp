#include "classmngr/engine/zip_archive_writer.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
namespace fs = std::filesystem;
namespace ZipArchiveWriter = classmngr::engine::ZipArchiveWriter;
using classmngr::engine::Clock;
using classmngr::engine::MonotonicTimePoint;
using classmngr::engine::WallClockTimePoint;

class FixedClock final : public Clock
{
public:
    [[nodiscard]] WallClockTimePoint nowUtc() const noexcept override
    {
        ++wallCalls;
        return wall;
    }

    [[nodiscard]] MonotonicTimePoint monotonicNow() const noexcept override
    {
        ++monotonicCalls;
        return monotonic;
    }

    WallClockTimePoint wall{};
    MonotonicTimePoint monotonic{};
    mutable int wallCalls = 0;
    mutable int monotonicCalls = 0;
};

struct TemporaryDirectory final
{
    fs::path path;

    TemporaryDirectory() = default;

    explicit TemporaryDirectory(fs::path value)
        : path(std::move(value))
    {
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    TemporaryDirectory(TemporaryDirectory&& other) noexcept
        : path(std::move(other.path))
    {
        other.path.clear();
    }

    TemporaryDirectory& operator=(TemporaryDirectory&& other) noexcept
    {
        if (this != &other)
        {
            std::error_code error;
            fs::remove_all(path, error);
            path = std::move(other.path);
            other.path.clear();
        }
        return *this;
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        fs::remove_all(path, error);
    }
};

struct ArchiveEntry final
{
    std::string name;
    std::string contents;
    std::uint16_t flags = 0;
    std::uint16_t compression = 0;
    std::uint32_t crc = 0;
};

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineZipArchiveWriterTests: "
              << message
              << '\n';
    return false;
}

std::string pathToUtf8(
    const fs::path& path
    )
{
    const std::u8string encoded = path.u8string();
    std::string result;
    result.reserve(encoded.size());
    for (const char8_t character : encoded)
    {
        result.push_back(static_cast<char>(character));
    }
    return result;
}

std::optional<TemporaryDirectory> makeTemporaryDirectory()
{
    std::error_code error;
    const fs::path root = fs::temp_directory_path(error);
    if (error)
    {
        return std::nullopt;
    }

    const auto suffix = std::chrono::steady_clock::now()
        .time_since_epoch()
        .count();
    TemporaryDirectory result(
        root / ("classmngr-engine-zip-" + std::to_string(suffix))
        );
    fs::create_directories(result.path, error);
    if (error)
    {
        return std::nullopt;
    }
    return result;
}

bool writeFile(
    const fs::path& path,
    std::string_view contents
    )
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open())
    {
        return false;
    }

    output.write(
        contents.data(),
        static_cast<std::streamsize>(contents.size())
        );
    return static_cast<bool>(output);
}

std::optional<std::string> readFile(
    const fs::path& path
    )
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open())
    {
        return std::nullopt;
    }

    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
        );
}

std::uint16_t readLe16(
    const std::string& data,
    std::size_t offset
    )
{
    return static_cast<std::uint16_t>(
        static_cast<unsigned char>(data[offset])
        | (static_cast<std::uint16_t>(
            static_cast<unsigned char>(data[offset + 1])
            ) << 8)
        );
}

std::uint32_t readLe32(
    const std::string& data,
    std::size_t offset
    )
{
    return static_cast<std::uint32_t>(
        static_cast<unsigned char>(data[offset])
        | (static_cast<std::uint32_t>(
            static_cast<unsigned char>(data[offset + 1])
            ) << 8)
        | (static_cast<std::uint32_t>(
            static_cast<unsigned char>(data[offset + 2])
            ) << 16)
        | (static_cast<std::uint32_t>(
            static_cast<unsigned char>(data[offset + 3])
            ) << 24)
        );
}

std::uint32_t crc32(
    std::string_view data
    )
{
    std::uint32_t crc = 0xffffffffu;
    for (const unsigned char character : data)
    {
        crc ^= character;
        for (int bit = 0; bit < 8; ++bit)
        {
            crc = (crc >> 1)
                ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

std::optional<std::vector<ArchiveEntry>> parseArchive(
    const fs::path& path
    )
{
    const auto bytes = readFile(path);
    if (!bytes || bytes->size() < 22)
    {
        return std::nullopt;
    }

    constexpr std::uint32_t EndOfCentralDirectorySignature = 0x06054b50;
    constexpr std::uint32_t CentralDirectoryHeaderSignature = 0x02014b50;
    constexpr std::uint32_t LocalFileHeaderSignature = 0x04034b50;

    std::optional<std::size_t> endOffset;
    for (std::size_t offset = bytes->size() - 22;; --offset)
    {
        if (readLe32(*bytes, offset) == EndOfCentralDirectorySignature)
        {
            endOffset = offset;
            break;
        }
        if (offset == 0)
        {
            break;
        }
    }
    if (!endOffset)
    {
        return std::nullopt;
    }

    const std::size_t centralOffset = readLe32(*bytes, *endOffset + 16);
    const std::size_t entryCount = readLe16(*bytes, *endOffset + 10);
    if (centralOffset > bytes->size())
    {
        return std::nullopt;
    }

    std::vector<ArchiveEntry> result;
    result.reserve(entryCount);
    std::size_t offset = centralOffset;
    for (std::size_t index = 0; index < entryCount; ++index)
    {
        if (offset > bytes->size()
            || bytes->size() - offset < 46
            || readLe32(*bytes, offset)
                != CentralDirectoryHeaderSignature)
        {
            return std::nullopt;
        }

        const std::size_t nameLength = readLe16(*bytes, offset + 28);
        const std::size_t extraLength = readLe16(*bytes, offset + 30);
        const std::size_t commentLength = readLe16(*bytes, offset + 32);
        const std::size_t centralLength =
            46 + nameLength + extraLength + commentLength;
        if (centralLength > bytes->size() - offset)
        {
            return std::nullopt;
        }

        const std::size_t localOffset = readLe32(*bytes, offset + 42);
        if (localOffset > bytes->size()
            || bytes->size() - localOffset < 30
            || readLe32(*bytes, localOffset)
                != LocalFileHeaderSignature)
        {
            return std::nullopt;
        }

        const std::size_t localNameLength =
            readLe16(*bytes, localOffset + 26);
        const std::size_t localExtraLength =
            readLe16(*bytes, localOffset + 28);
        const std::size_t contentOffset =
            localOffset + 30 + localNameLength + localExtraLength;
        const std::size_t contentLength = readLe32(*bytes, offset + 20);
        if (contentOffset > bytes->size()
            || contentLength > bytes->size() - contentOffset)
        {
            return std::nullopt;
        }

        ArchiveEntry entry;
        entry.name = bytes->substr(offset + 46, nameLength);
        entry.contents = bytes->substr(contentOffset, contentLength);
        entry.flags = readLe16(*bytes, offset + 8);
        entry.compression = readLe16(*bytes, offset + 10);
        entry.crc = readLe32(*bytes, offset + 16);
        result.push_back(std::move(entry));
        offset += centralLength;
    }

    return result;
}

bool noTemporaryZipFiles(
    const fs::path& directory,
    std::string_view targetName
    )
{
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator(
             directory,
             error
             ))
    {
        if (error)
        {
            return false;
        }

        const std::string name = pathToUtf8(entry.path().filename());
        if (name.starts_with(std::string(targetName)
                             + ".classmngr-zip-temp-"))
        {
            return false;
        }
    }
    return !error;
}
} // namespace

int main()
{
    const auto temporaryDirectory = makeTemporaryDirectory();
    if (!temporaryDirectory)
    {
        std::cerr << "ClassMngrEngineZipArchiveWriterTests: "
                  << "could not create a temporary directory\n";
        return 1;
    }

    const fs::path firstSource = temporaryDirectory->path / "first.txt";
    const fs::path secondSource = temporaryDirectory->path / "두 번째.txt";
    const std::string firstContents = "first report / 첫 번째\n";
    const std::string secondContents = "second report / 두 번째\n";
    bool passed = true;
    passed &= expect(
        writeFile(firstSource, firstContents)
            && writeFile(secondSource, secondContents),
        "could not create source files"
        );

    const fs::path archivePath = temporaryDirectory->path / "reports.zip";
    FixedClock clock;
    clock.wall = WallClockTimePoint(std::chrono::seconds(1'930'899'600));
    clock.monotonic = MonotonicTimePoint(std::chrono::milliseconds(456));
    const auto written = ZipArchiveWriter::writeArchive(
        pathToUtf8(archivePath),
        {
            {pathToUtf8(firstSource), "학생/첫 번째.txt"},
            {pathToUtf8(secondSource), "student-2.txt"}
        },
        clock
        );
    passed &= expect(
        written.has_value()
            && clock.wallCalls >= 2
            && clock.monotonicCalls == 1,
        "archive creation ignored the injected clock"
        );

    const auto archive = parseArchive(archivePath);
    passed &= expect(archive.has_value(), "valid archive could not be parsed");
    if (archive)
    {
        passed &= expect(archive->size() == 2, "archive entry count changed");
        if (archive->size() == 2)
        {
            passed &= expect(
                archive->at(0).name == "학생/첫 번째.txt"
                    && archive->at(1).name == "student-2.txt",
                "archive entry ordering or UTF-8 names changed"
                );
            passed &= expect(
                archive->at(0).contents == firstContents
                    && archive->at(1).contents == secondContents,
                "archive entry contents changed"
                );
            for (const ArchiveEntry& entry : *archive)
            {
                passed &= expect(
                    (entry.flags & 0x0800u) != 0
                        && entry.compression == 0
                        && entry.crc == crc32(entry.contents),
                    "archive entry ZIP metadata is incompatible"
                    );
            }
        }
    }

    const std::vector<std::string> invalidNames{
        "",
        "   ",
        "/absolute.txt",
        "\\absolute.txt",
        "a//b.txt",
        "a/./b.txt",
        "a/../b.txt",
        "C:/absolute.txt"
    };
    for (std::size_t index = 0; index < invalidNames.size(); ++index)
    {
        const fs::path invalidArchive = temporaryDirectory->path
            / ("invalid-" + std::to_string(index) + ".zip");
        const auto result = ZipArchiveWriter::writeArchive(
            pathToUtf8(invalidArchive),
            {{pathToUtf8(firstSource), invalidNames.at(index)}}
            );
        passed &= expect(
            !result && !fs::exists(invalidArchive),
            "invalid archive name was accepted or left output behind"
            );
    }

    const fs::path duplicateArchive = temporaryDirectory->path
        / "duplicate.zip";
    const auto duplicate = ZipArchiveWriter::writeArchive(
        pathToUtf8(duplicateArchive),
        {
            {pathToUtf8(firstSource), "same.txt"},
            {pathToUtf8(secondSource), "same.txt"}
        }
        );
    passed &= expect(
        !duplicate && !fs::exists(duplicateArchive),
        "duplicate archive names were accepted or left output behind"
        );

    const fs::path missingArchive = temporaryDirectory->path / "missing.zip";
    const auto missing = ZipArchiveWriter::writeArchive(
        pathToUtf8(missingArchive),
        {{pathToUtf8(temporaryDirectory->path / "missing.pdf"), "missing.pdf"}}
        );
    passed &= expect(
        !missing && !fs::exists(missingArchive),
        "missing source file was accepted or left output behind"
        );

    const fs::path existingArchive = temporaryDirectory->path / "existing.zip";
    passed &= expect(
        ZipArchiveWriter::writeArchive(
            pathToUtf8(existingArchive),
            {{pathToUtf8(firstSource), "initial.txt"}}
            ).has_value(),
        "could not create replacement baseline"
        );
    const auto beforeReplacement = readFile(existingArchive);
    const auto failedReplacement = ZipArchiveWriter::writeArchive(
        pathToUtf8(existingArchive),
        {{pathToUtf8(temporaryDirectory->path / "gone.pdf"), "gone.pdf"}}
        );
    const auto afterReplacement = readFile(existingArchive);
    passed &= expect(
        !failedReplacement
            && beforeReplacement.has_value()
            && afterReplacement == beforeReplacement,
        "failed replacement did not preserve the existing archive"
        );

    const fs::path blockedTarget = temporaryDirectory->path / "blocked";
    std::error_code directoryError;
    fs::create_directory(blockedTarget, directoryError);
    const auto blocked = ZipArchiveWriter::writeArchive(
        pathToUtf8(blockedTarget),
        {{pathToUtf8(firstSource), "first.txt"}}
        );
    passed &= expect(
        !blocked
            && fs::is_directory(blockedTarget)
            && noTemporaryZipFiles(temporaryDirectory->path, "blocked"),
        "output failure left a temporary archive behind"
        );

    return passed ? 0 : 1;
}
