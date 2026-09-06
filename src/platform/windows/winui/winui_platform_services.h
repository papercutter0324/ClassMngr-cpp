#pragma once

#include "classmngr/engine/file_system.h"
#include "classmngr/engine/platform_services.h"

#include <optional>
#include <string>
#include <string_view>

// Windows-only implementations used by the unpackaged WinUI shell.  The
// public boundary deliberately remains UTF-8 and engine::Result based.
namespace classmngr::windows::winui
{

class WindowsSettingsStore final : public engine::SettingsStore
{
public:
    // Values are kept below HKCU\Software\PaperCloud\ClassMngrWinUI by
    // default.  A caller may select a subkey for an isolated profile/test.
    explicit WindowsSettingsStore(
        std::wstring subkey = L"Software\\PaperCloud\\ClassMngrWinUI"
        );

    [[nodiscard]] engine::Result<std::optional<engine::PlatformSettingValue>> read(
        std::string_view key
        ) const override;
    [[nodiscard]] engine::Status write(
        std::string_view key,
        engine::PlatformSettingValue const& value
        ) override;
    [[nodiscard]] engine::Status remove(std::string_view key) override;

private:
    std::wstring m_subkey;
};

// DPAPI current-user protection for secrets that have a separate storage
// policy.  This class intentionally does not decide where secrets are stored.
class WindowsSecureStorage final
{
public:
    [[nodiscard]] static engine::Result<engine::ByteBuffer> protect(
        engine::ByteBuffer const& plaintext
        );
    [[nodiscard]] static engine::Result<engine::ByteBuffer> unprotect(
        engine::ByteBuffer const& protectedBytes
        );
};

// File and directory behaviour stays in the portable StandardFileSystem.
// This named adapter makes that deliberate delegation explicit at the WinUI
// composition boundary while retaining its UTF-8 paths and typed errors.
class WindowsFileSystem final : public engine::FileSystem
{
public:
    [[nodiscard]] engine::Result<std::string> normalizePath(std::string_view utf8Path) const override;
    [[nodiscard]] engine::Result<bool> exists(std::string_view utf8Path) const override;
    [[nodiscard]] engine::Status createDirectories(std::string_view utf8DirectoryPath) const override;
    [[nodiscard]] engine::Result<std::string> readBytes(std::string_view utf8Path) const override;
    [[nodiscard]] engine::Status writeBytes(std::string_view utf8Path, std::string_view bytes, bool createParentDirectories = false) const override;
    [[nodiscard]] engine::Status copyFile(std::string_view utf8SourcePath, std::string_view utf8DestinationPath, bool createParentDirectories = false) const override;
    [[nodiscard]] engine::Status moveFile(std::string_view utf8SourcePath, std::string_view utf8DestinationPath) const override;
    [[nodiscard]] engine::Status replaceFileAtomically(std::string_view utf8TemporaryPath, std::string_view utf8DestinationPath) const override;
    [[nodiscard]] engine::Status replaceDirectoryAtomically(std::string_view utf8TemporaryDirectoryPath, std::string_view utf8DestinationDirectoryPath) const override;
    [[nodiscard]] engine::Status removeFile(std::string_view utf8Path) const override;
    [[nodiscard]] engine::Result<std::string> createTemporaryDirectory(std::string_view utf8ParentDirectory) const override;
    [[nodiscard]] engine::Status removeTemporaryDirectory(std::string_view utf8DirectoryPath) const override;

private:
    engine::StandardFileSystem m_fileSystem;
};

class WindowsClipboard final
{
public:
    [[nodiscard]] static engine::Result<std::string> readText() noexcept;
    [[nodiscard]] static engine::Status writeText(std::string_view utf8Text) noexcept;
};

class WindowsUrlLauncher final
{
public:
    [[nodiscard]] static engine::Status openUrl(std::string_view utf8Url) noexcept;
};

class WindowsProcessLauncher final : public engine::ProcessLauncher
{
public:
    [[nodiscard]] engine::Result<engine::ProcessResult> launch(
        engine::ProcessLaunchRequest const& request
        ) override;
    [[nodiscard]] engine::Status launchDetached(
        engine::ProcessLaunchRequest const& request
        ) override;
};

// Notification registration deliberately remains explicit: callers that have
// not completed WinUI activation wiring get a typed Unsupported result instead
// of an apparent success.
class WindowsNotificationSink final
{
public:
    [[nodiscard]] engine::Status registerForAppNotifications() noexcept;
    [[nodiscard]] engine::Status show(std::string_view xmlPayload) noexcept;

private:
    bool m_registered = false;
};

class WindowsLogger final : public engine::Logger
{
public:
    // An empty path selects %LOCALAPPDATA%\PaperCloud\ClassMngrWinUI\Logs\app.log.
    explicit WindowsLogger(std::wstring utf16LogFile = {});
    void log(engine::LogRecord const& record) noexcept override;

private:
    std::wstring m_logFile;
};

class WindowsCrashDiagnostics final
{
public:
    // Installs a process-wide best-effort unhandled-exception filter.  It also
    // calls the existing lifecycle fatal reporter, preserving its log path.
    [[nodiscard]] static engine::Status install() noexcept;
};

// Pure conversion/error-path coverage.  It does not touch the registry,
// clipboard, notification APIs, process state, or crash-filter state.
[[nodiscard]] bool runPlatformAdapterContractChecks() noexcept;

} // namespace classmngr::windows::winui
