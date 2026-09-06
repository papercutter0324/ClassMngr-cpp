#include "classmngr/engine/file_system.h"
#include "classmngr/engine/platform_services.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace
{

namespace fs = std::filesystem;
using classmngr::engine::ErrorCode;
using classmngr::engine::Clock;
using classmngr::engine::MonotonicTimePoint;
using classmngr::engine::StandardFileSystem;
using classmngr::engine::WallClockTimePoint;
namespace FileSystemErrorToken = classmngr::engine::FileSystemErrorToken;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineFileSystemTests: " << message << '\n';
    return false;
}

std::string pathToUtf8(
    const fs::path& path
    )
{
    const std::u8string encoded = path.u8string();
    return std::string(
        reinterpret_cast<const char*>(encoded.data()),
        encoded.size()
        );
}

bool hasTemporaryArtifacts(
    const fs::path& root
    )
{
    std::error_code error;
    fs::recursive_directory_iterator iterator(root, error);
    const fs::recursive_directory_iterator end;
    while (!error && iterator != end)
    {
        const std::string name = pathToUtf8(iterator->path().filename());
        if (name.find(".classmngr-file-system-") != std::string::npos)
        {
            return true;
        }
        iterator.increment(error);
    }

    return false;
}

bool hasErrorToken(
    const auto& result,
    ErrorCode code,
    std::string_view token
    )
{
    return !result
        && result.error().code == code
        && result.error().message == token;
}

class TemporaryDirectoryGuard final
{
public:
    TemporaryDirectoryGuard(
        const StandardFileSystem& fileSystem,
        std::string path
        )
        : m_fileSystem(fileSystem)
        , m_path(std::move(path))
    {
    }

    ~TemporaryDirectoryGuard()
    {
        if (!m_path.empty())
        {
            (void)m_fileSystem.removeTemporaryDirectory(m_path);
        }
    }

    const std::string& path() const noexcept
    {
        return m_path;
    }

private:
    const StandardFileSystem& m_fileSystem;
    std::string m_path;
};

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

} // namespace

int main()
{
    bool passed = true;
    StandardFileSystem fileSystem;

    const auto relative = fileSystem.normalizePath(
        "classmngr-relative-file.bin"
        );
    passed &= expect(
        relative
            && fs::u8path(*relative).is_absolute()
            && fs::u8path(*relative).filename() == "classmngr-relative-file.bin",
        "relative UTF-8 paths were not normalized to absolute paths"
        );

    const auto blank = fileSystem.normalizePath(" \t\n");
    passed &= expect(
        hasErrorToken(
            blank,
            ErrorCode::InvalidArgument,
            FileSystemErrorToken::InvalidPath
            ),
        "blank paths did not return a typed invalid-path error"
        );

    const std::string embeddedNull("bad\0path", 8);
    const auto invalid = fileSystem.normalizePath(embeddedNull);
    passed &= expect(
        hasErrorToken(
            invalid,
            ErrorCode::InvalidArgument,
            FileSystemErrorToken::InvalidPath
            ),
        "embedded-null paths did not return a typed invalid-path error"
        );

    std::error_code temporaryRootError;
    const fs::path systemTemporaryDirectory = fs::temp_directory_path(
        temporaryRootError
        );
    passed &= expect(
        !temporaryRootError,
        "the host temporary directory could not be resolved"
        );
    if (temporaryRootError)
    {
        return 1;
    }

    FixedClock clock;
    clock.monotonic = MonotonicTimePoint(std::chrono::milliseconds(456));
    StandardFileSystem fixedClockFileSystem(clock);
    const auto fixedClockTemporaryRoot =
        fixedClockFileSystem.createTemporaryDirectory(
            pathToUtf8(systemTemporaryDirectory)
            );
    passed &= expect(
        fixedClockTemporaryRoot
            && clock.monotonicCalls == 1
            && fixedClockTemporaryRoot->find(".classmngr-temporary-")
                != std::string::npos
            && fixedClockFileSystem.removeTemporaryDirectory(
                *fixedClockTemporaryRoot
                ).has_value(),
        "temporary directory naming ignored the injected clock"
        );

    const auto temporaryRoot = fileSystem.createTemporaryDirectory(
        pathToUtf8(systemTemporaryDirectory)
        );
    passed &= expect(
        temporaryRoot.has_value(),
        "temporary directory creation failed"
        );
    if (!temporaryRoot)
    {
        return 1;
    }
    TemporaryDirectoryGuard temporaryRootGuard(fileSystem, *temporaryRoot);
    const fs::path root = fs::u8path(temporaryRootGuard.path());

    const std::string koreanDirectory =
        "\xed\x8c\x8c\xec\x9d\xbc-\xed\x85\x8c\xec\x8a\xa4\xed\x8a\xb8";
    const std::string sourcePath = pathToUtf8(
        root / koreanDirectory / "profile.bin"
        );
    const std::string destinationPath = pathToUtf8(
        root / "export" / koreanDirectory / "profile-copy.bin"
        );
    const std::string payload(
        "\0Teacher Profile\xff\x01\x80",
        20
        );

    const auto createdDirectory = fileSystem.createDirectories(
        pathToUtf8(root / "explicit" / koreanDirectory)
        );
    passed &= expect(
        createdDirectory.has_value(),
        "nested UTF-8 directory creation failed"
        );
    const auto explicitDirectoryExists = fileSystem.exists(
        pathToUtf8(root / "explicit" / koreanDirectory)
        );
    passed &= expect(
        explicitDirectoryExists && *explicitDirectoryExists,
        "created UTF-8 directory was not observable through exists"
        );

    const auto written = fileSystem.writeBytes(sourcePath, payload, true);
    passed &= expect(
        written.has_value(),
        "byte payload write did not create its parent directory"
        );
    const auto sourceExists = fileSystem.exists(sourcePath);
    passed &= expect(
        sourceExists && *sourceExists,
        "written UTF-8 source file was not observable through exists"
        );
    const auto readSource = fileSystem.readBytes(sourcePath);
    passed &= expect(
        readSource && *readSource == payload,
        "byte payload round trip did not preserve arbitrary bytes"
        );

    const auto copied = fileSystem.copyFile(
        sourcePath,
        destinationPath,
        true
        );
    passed &= expect(
        copied.has_value(),
        "atomic UTF-8 file copy failed"
        );
    const auto readDestination = fileSystem.readBytes(destinationPath);
    passed &= expect(
        readDestination && *readDestination == payload,
        "copied UTF-8 destination did not preserve source bytes"
        );

    const auto sameSource = fileSystem.copyFile(
        sourcePath,
        sourcePath,
        true
        );
    passed &= expect(
        sameSource.has_value(),
        "same-source copy did not remain a no-op"
        );

    const std::string oldPayload = "old destination";
    const auto oldWritten = fileSystem.writeBytes(
        destinationPath,
        oldPayload
        );
    passed &= expect(
        oldWritten.has_value(),
        "existing destination fixture could not be written"
        );
    const auto overwritten = fileSystem.copyFile(
        sourcePath,
        destinationPath,
        true
        );
    passed &= expect(
        overwritten.has_value(),
        "existing destination overwrite failed"
        );
    const auto readOverwritten = fileSystem.readBytes(destinationPath);
    passed &= expect(
        readOverwritten && *readOverwritten == payload,
        "destination overwrite did not finalize the copied bytes"
        );

    const std::string missingSourcePath = pathToUtf8(
        root / "missing-source.bin"
        );
    const std::string missingDestinationPath = pathToUtf8(
        root / "missing-destination.bin"
        );
    const auto missingSource = fileSystem.copyFile(
        missingSourcePath,
        missingDestinationPath,
        true
        );
    passed &= expect(
        hasErrorToken(
            missingSource,
            ErrorCode::NotFound,
            FileSystemErrorToken::MissingSource
            ),
        "missing source did not return a typed missing-source error"
        );
    const auto missingDestinationExists = fileSystem.exists(
        missingDestinationPath
        );
    passed &= expect(
        missingDestinationExists && !*missingDestinationExists,
        "failed missing-source copy left a destination"
        );

    const std::string nonRegularSourcePath = pathToUtf8(
        root / "directory-source"
        );
    passed &= expect(
        fileSystem.createDirectories(nonRegularSourcePath).has_value(),
        "non-regular source fixture could not be created"
        );
    const auto nonRegularSource = fileSystem.copyFile(
        nonRegularSourcePath,
        pathToUtf8(root / "non-regular-destination.bin"),
        true
        );
    passed &= expect(
        hasErrorToken(
            nonRegularSource,
            ErrorCode::InvalidArgument,
            FileSystemErrorToken::SourceNotRegular
            ),
        "non-regular source did not return a typed source error"
        );

    const std::string blockedParentPath = pathToUtf8(
        root / "blocked-parent"
        );
    passed &= expect(
        fileSystem.writeBytes(blockedParentPath, "blocker").has_value(),
        "blocked-parent fixture could not be created"
        );
    const auto blockedDestination = fileSystem.copyFile(
        sourcePath,
        pathToUtf8(root / "blocked-parent" / "destination.bin"),
        true
        );
    passed &= expect(
        hasErrorToken(
            blockedDestination,
            ErrorCode::Io,
            FileSystemErrorToken::DirectoryCreationFailed
            ),
        "blocked destination parent did not return a typed directory error"
        );

    const std::string directoryDestinationPath = pathToUtf8(
        root / "destination-directory"
        );
    passed &= expect(
        fileSystem.createDirectories(directoryDestinationPath).has_value(),
        "replacement-failure destination directory could not be created"
        );
    const std::string directoryMarkerPath = pathToUtf8(
        root / "destination-directory" / "marker.bin"
        );
    passed &= expect(
        fileSystem.writeBytes(directoryMarkerPath, "keep me").has_value(),
        "replacement-failure marker could not be created"
        );
    const auto replacementFailure = fileSystem.copyFile(
        sourcePath,
        directoryDestinationPath,
        true
        );
    passed &= expect(
        hasErrorToken(
            replacementFailure,
            ErrorCode::Io,
            FileSystemErrorToken::AtomicReplacementFailed
            ),
        "directory destination did not return a typed atomic-replacement error"
        );
    const auto preservedMarker = fileSystem.readBytes(directoryMarkerPath);
    passed &= expect(
        preservedMarker && *preservedMarker == "keep me",
        "failed replacement changed the existing destination directory"
        );
    passed &= expect(
        !hasTemporaryArtifacts(root),
        "failed operations left a temporary or backup artifact"
        );

    const std::string stagedPath = pathToUtf8(root / "staged.bin");
    const std::string finalizedPath = pathToUtf8(root / "finalized.bin");
    passed &= expect(
        fileSystem.writeBytes(stagedPath, payload).has_value(),
        "temporary replacement fixture could not be staged"
        );
    const auto finalized = fileSystem.replaceFileAtomically(
        stagedPath,
        finalizedPath
        );
    passed &= expect(
        finalized.has_value(),
        "explicit atomic replacement failed"
        );
    const auto finalizedExists = fileSystem.exists(finalizedPath);
    const auto stagedExists = fileSystem.exists(stagedPath);
    passed &= expect(
        finalizedExists && *finalizedExists
            && stagedExists && !*stagedExists,
        "atomic replacement did not finalize and consume its temporary path"
        );

    const auto temporaryChild = fileSystem.createTemporaryDirectory(
        temporaryRootGuard.path()
        );
    passed &= expect(
        temporaryChild.has_value(),
        "nested temporary directory creation failed"
        );
    if (temporaryChild)
    {
        const std::string childFile = pathToUtf8(
            fs::u8path(*temporaryChild) / "child.bin"
            );
        passed &= expect(
            fileSystem.writeBytes(childFile, "child").has_value(),
            "temporary directory cleanup fixture could not be written"
            );
        const auto removedChild = fileSystem.removeTemporaryDirectory(
            *temporaryChild
            );
        passed &= expect(
            removedChild.has_value(),
            "temporary directory cleanup failed"
            );
        const auto childExists = fileSystem.exists(*temporaryChild);
        passed &= expect(
            childExists && !*childExists,
            "temporary directory cleanup left its directory behind"
            );
    }

    const auto directoryStage = fileSystem.createTemporaryDirectory(
        temporaryRootGuard.path()
        );
    const std::string directoryDestination = pathToUtf8(
        root / "package-output"
        );
    passed &= expect(
        directoryStage
            && fileSystem.createDirectories(
                pathToUtf8(fs::u8path(*directoryStage) / "new-file")
                ).has_value()
            && fileSystem.createDirectories(directoryDestination).has_value(),
        "directory replacement fixture could not be prepared"
        );
    if (directoryStage)
    {
        const std::string oldMarker = pathToUtf8(
            fs::u8path(directoryDestination) / "old-file"
            );
        const std::string newMarker = pathToUtf8(
            fs::u8path(*directoryStage) / "new-file" / "marker"
            );
        const std::string finalMarker = pathToUtf8(
            fs::u8path(directoryDestination) / "new-file" / "marker"
            );
        passed &= expect(
            fileSystem.writeBytes(oldMarker, "old").has_value()
                && fileSystem.writeBytes(newMarker, "new").has_value(),
            "directory replacement marker could not be written"
            );

        const auto replacedDirectory =
            fileSystem.replaceDirectoryAtomically(
                *directoryStage,
                directoryDestination
                );
        passed &= expect(
            replacedDirectory.has_value(),
            "atomic directory replacement failed"
            );
        const auto replacementMarker = fileSystem.readBytes(finalMarker);
        const auto oldMarkerAfterReplacement = fileSystem.readBytes(oldMarker);
        passed &= expect(
            replacementMarker && *replacementMarker == "new"
                && !oldMarkerAfterReplacement,
            "directory replacement did not replace the existing package"
            );
    }

    return passed ? 0 : 1;
}
