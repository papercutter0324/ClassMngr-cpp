#include "pch.h"

#include "winui_platform_services.h"

#include "winui_lifecycle.h"

#include <winrt/Microsoft.Windows.AppNotifications.h>

#include <bcrypt.h>
#include <dbghelp.h>
#include <shellapi.h>
#include <wincrypt.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace classmngr::windows::winui
{
namespace
{
using engine::ByteBuffer;
using engine::Error;
using engine::ErrorCode;
using engine::PlatformSettingValue;

constexpr std::array<std::byte, 4> settingMagic{
    std::byte{'C'}, std::byte{'M'}, std::byte{'S'}, std::byte{'1'}
};
constexpr std::size_t settingHeaderSize = settingMagic.size() + 1;
constexpr wchar_t applicationLogName[] = L"app.log";
constexpr wchar_t crashDumpName[] = L"unhandled-exception.dmp";

enum class StoredSettingType : unsigned char
{
    Boolean = 1,
    Integer = 2,
    Double = 3,
    String = 4,
    Bytes = 5,
};

Error failure(
    ErrorCode code,
    std::string message,
    std::optional<int> nativeCode = std::nullopt
    )
{
    return Error{code, std::move(message), nativeCode};
}

Error windowsFailure(std::string message, DWORD error = GetLastError())
{
    return failure(ErrorCode::Io, std::move(message), static_cast<int>(error));
}

engine::Result<std::wstring> toWide(std::string_view value, char const* what)
{
    if (value.empty())
    {
        return std::wstring{};
    }
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return std::unexpected(failure(ErrorCode::NumericOverflow, std::string(what) + " is too large."));
    }
    const int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0)
    {
        return std::unexpected(windowsFailure(std::string(what) + " is not valid UTF-8."));
    }
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), length) <= 0)
    {
        return std::unexpected(windowsFailure(std::string(what) + " is not valid UTF-8."));
    }
    return result;
}

engine::Result<std::string> toUtf8(std::wstring_view value, char const* what)
{
    if (value.empty())
    {
        return std::string{};
    }
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return std::unexpected(failure(ErrorCode::NumericOverflow, std::string(what) + " is too large."));
    }
    const int length = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0)
    {
        return std::unexpected(windowsFailure(std::string(what) + " cannot be converted to UTF-8."));
    }
    std::string result(static_cast<std::size_t>(length), '\0');
    if (WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), length, nullptr, nullptr) <= 0)
    {
        return std::unexpected(windowsFailure(std::string(what) + " cannot be converted to UTF-8."));
    }
    return result;
}

bool validRegistryValueName(std::wstring_view key) noexcept
{
    return !key.empty() && key.find(L'\\') == std::wstring_view::npos && key.find(L'\0') == std::wstring_view::npos;
}

engine::Result<std::vector<std::byte>> encodeSetting(PlatformSettingValue const& value)
{
    return std::visit(
        [](auto const& item) -> engine::Result<std::vector<std::byte>>
        {
            using Value = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<Value, std::monostate>)
            {
                return std::unexpected(failure(ErrorCode::InvalidArgument, "A monostate setting cannot be stored."));
            }
            else
            {
                StoredSettingType type{};
                std::vector<std::byte> payload;
                if constexpr (std::is_same_v<Value, bool>)
                {
                    type = StoredSettingType::Boolean;
                    payload = {item ? std::byte{1} : std::byte{0}};
                }
                else if constexpr (std::is_same_v<Value, std::int64_t> || std::is_same_v<Value, double>)
                {
                    type = std::is_same_v<Value, std::int64_t> ? StoredSettingType::Integer : StoredSettingType::Double;
                    payload.resize(sizeof(item));
                    std::memcpy(payload.data(), &item, sizeof(item));
                }
                else if constexpr (std::is_same_v<Value, std::string>)
                {
                    const auto wide = toWide(item, "The setting string");
                    if (!wide)
                    {
                        return std::unexpected(wide.error());
                    }
                    type = StoredSettingType::String;
                    payload.resize(item.size());
                    std::memcpy(payload.data(), item.data(), item.size());
                }
                else
                {
                    type = StoredSettingType::Bytes;
                    payload.assign(item.begin(), item.end());
                }

                std::vector<std::byte> encoded;
                encoded.reserve(settingHeaderSize + payload.size());
                encoded.insert(encoded.end(), settingMagic.begin(), settingMagic.end());
                encoded.push_back(static_cast<std::byte>(type));
                encoded.insert(encoded.end(), payload.begin(), payload.end());
                return encoded;
            }
        }, value);
}

engine::Result<PlatformSettingValue> decodeSetting(std::vector<std::byte> const& encoded)
{
    if (encoded.size() < settingHeaderSize
        || !std::equal(settingMagic.begin(), settingMagic.end(), encoded.begin()))
    {
        return std::unexpected(failure(ErrorCode::InvalidFormat, "The registry setting has an unsupported format."));
    }
    const auto type = static_cast<StoredSettingType>(
        std::to_integer<unsigned char>(encoded[settingMagic.size()])
        );
    const std::byte* payload = encoded.data() + settingHeaderSize;
    const std::size_t size = encoded.size() - settingHeaderSize;
    switch (type)
    {
    case StoredSettingType::Boolean:
        if (size != 1 || (payload[0] != std::byte{0} && payload[0] != std::byte{1}))
        {
            return std::unexpected(failure(ErrorCode::InvalidFormat, "The registry boolean setting is invalid."));
        }
        return PlatformSettingValue{payload[0] == std::byte{1}};
    case StoredSettingType::Integer:
    {
        if (size != sizeof(std::int64_t))
        {
            return std::unexpected(failure(ErrorCode::InvalidFormat, "The registry integer setting is invalid."));
        }
        std::int64_t value{};
        std::memcpy(&value, payload, sizeof(value));
        return PlatformSettingValue{value};
    }
    case StoredSettingType::Double:
    {
        if (size != sizeof(double))
        {
            return std::unexpected(failure(ErrorCode::InvalidFormat, "The registry floating-point setting is invalid."));
        }
        double value{};
        std::memcpy(&value, payload, sizeof(value));
        return PlatformSettingValue{value};
    }
    case StoredSettingType::String:
    {
        std::string value(reinterpret_cast<char const*>(payload), size);
        const auto validated = toWide(value, "The registry string setting");
        if (!validated)
        {
            return std::unexpected(validated.error());
        }
        return PlatformSettingValue{std::move(value)};
    }
    case StoredSettingType::Bytes:
        return PlatformSettingValue{ByteBuffer(payload, payload + size)};
    }
    return std::unexpected(failure(ErrorCode::InvalidFormat, "The registry setting has an unsupported type."));
}

class RegistryKey final
{
public:
    RegistryKey() = default;
    explicit RegistryKey(HKEY key) noexcept : m_key(key) {}
    RegistryKey(RegistryKey const&) = delete;
    RegistryKey& operator=(RegistryKey const&) = delete;
    RegistryKey(RegistryKey&& other) noexcept : m_key(std::exchange(other.m_key, nullptr)) {}
    RegistryKey& operator=(RegistryKey&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            m_key = std::exchange(other.m_key, nullptr);
        }
        return *this;
    }
    ~RegistryKey() { reset(); }
    [[nodiscard]] HKEY get() const noexcept { return m_key; }
    void reset() noexcept
    {
        if (m_key)
        {
            RegCloseKey(m_key);
            m_key = nullptr;
        }
    }
private:
    HKEY m_key = nullptr;
};

engine::Result<RegistryKey> openRegistryKey(std::wstring const& subkey, REGSAM access, bool create)
{
    HKEY key{};
    DWORD disposition{};
    const LSTATUS status = create
        ? RegCreateKeyExW(HKEY_CURRENT_USER, subkey.c_str(), 0, nullptr, 0, access, nullptr, &key, &disposition)
        : RegOpenKeyExW(HKEY_CURRENT_USER, subkey.c_str(), 0, access, &key);
    if (status != ERROR_SUCCESS)
    {
        return std::unexpected(failure(
            status == ERROR_FILE_NOT_FOUND ? ErrorCode::NotFound : ErrorCode::Io,
            "Unable to open the Windows settings registry key.", static_cast<int>(status)));
    }
    return RegistryKey{key};
}

std::wstring localLogDirectory() noexcept
{
    wchar_t path[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", path, static_cast<DWORD>(std::size(path)));
    if (length == 0 || length >= std::size(path))
    {
        return {};
    }
    std::wstring result(path, length);
    result += L"\\PaperCloud\\ClassMngrWinUI\\Logs";
    CreateDirectoryW((std::wstring(path, length) + L"\\PaperCloud").c_str(), nullptr);
    CreateDirectoryW((std::wstring(path, length) + L"\\PaperCloud\\ClassMngrWinUI").c_str(), nullptr);
    CreateDirectoryW(result.c_str(), nullptr);
    return result;
}

std::wstring defaultLogFile() noexcept
{
    std::wstring directory = localLogDirectory();
    return directory.empty() ? std::wstring{} : directory + L"\\" + applicationLogName;
}

std::wstring quoteCommandLineArgument(std::wstring_view argument)
{
    if (argument.find_first_of(L" \t\n\v\"") == std::wstring_view::npos)
    {
        return std::wstring(argument);
    }
    std::wstring result(L"\"");
    std::size_t slashes{};
    for (wchar_t character : argument)
    {
        if (character == L'\\')
        {
            ++slashes;
        }
        else if (character == L'\"')
        {
            result.append(slashes * 2 + 1, L'\\');
            result.push_back(character);
            slashes = 0;
        }
        else
        {
            result.append(slashes, L'\\');
            slashes = 0;
            result.push_back(character);
        }
    }
    result.append(slashes * 2, L'\\');
    result.push_back(L'\"');
    return result;
}

engine::Result<std::pair<std::wstring, std::wstring>> processCommandLine(engine::ProcessLaunchRequest const& request)
{
    if (request.executable.empty() || request.timeout.count() < 0)
    {
        return std::unexpected(failure(ErrorCode::InvalidArgument, "The process launch request is invalid."));
    }
    const auto executable = toWide(request.executable, "The process executable");
    if (!executable || executable->empty())
    {
        return std::unexpected(executable ? failure(ErrorCode::InvalidArgument, "The process executable is empty.") : executable.error());
    }
    std::wstring commandLine = quoteCommandLineArgument(*executable);
    for (std::string const& argument : request.arguments)
    {
        const auto wideArgument = toWide(argument, "A process argument");
        if (!wideArgument)
        {
            return std::unexpected(wideArgument.error());
        }
        commandLine += L" ";
        commandLine += quoteCommandLineArgument(*wideArgument);
    }
    return std::pair{*executable, std::move(commandLine)};
}

engine::Result<std::optional<std::wstring>> processWorkingDirectory(engine::ProcessLaunchRequest const& request)
{
    if (request.workingDirectory.empty())
    {
        return std::optional<std::wstring>{};
    }
    const auto directory = toWide(request.workingDirectory, "The process working directory");
    if (!directory)
    {
        return std::unexpected(directory.error());
    }
    return std::optional<std::wstring>{*directory};
}

bool cancelled(engine::CancellationToken const* token) noexcept
{
    return token && token->isCancellationRequested();
}

engine::Status cancelledStatus(char const* message)
{
    return std::unexpected(failure(ErrorCode::Cancelled, message));
}

void drainPipe(HANDLE pipe, ByteBuffer& destination) noexcept
{
    while (true)
    {
        DWORD available{};
        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) || available == 0)
        {
            return;
        }
        const DWORD count = std::min<DWORD>(available, 4096);
        std::array<std::byte, 4096> buffer{};
        DWORD read{};
        if (!ReadFile(pipe, buffer.data(), count, &read, nullptr) || read == 0)
        {
            return;
        }
        try
        {
            destination.insert(destination.end(), buffer.begin(), buffer.begin() + read);
        }
        catch (...)
        {
            return;
        }
    }
}

std::wstring severityName(engine::LogSeverity severity)
{
    switch (severity)
    {
    case engine::LogSeverity::Trace: return L"trace";
    case engine::LogSeverity::Debug: return L"debug";
    case engine::LogSeverity::Info: return L"info";
    case engine::LogSeverity::Warning: return L"warning";
    case engine::LogSeverity::Error: return L"error";
    case engine::LogSeverity::Critical: return L"critical";
    }
    return L"unknown";
}

LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionPointers) noexcept
{
    ClassMngrWinUILifecycle::reportFatalError(L"Unhandled Windows exception.");
    const std::wstring directory = localLogDirectory();
    if (!directory.empty())
    {
        const std::wstring dumpPath = directory + L"\\" + crashDumpName;
        HANDLE const dump = CreateFileW(dumpPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (dump != INVALID_HANDLE_VALUE)
        {
            MINIDUMP_EXCEPTION_INFORMATION exceptionInfo{};
            exceptionInfo.ThreadId = GetCurrentThreadId();
            exceptionInfo.ExceptionPointers = exceptionPointers;
            exceptionInfo.ClientPointers = FALSE;
            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dump, MiniDumpNormal, &exceptionInfo, nullptr, nullptr);
            CloseHandle(dump);
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

WindowsSettingsStore::WindowsSettingsStore(std::wstring subkey)
    : m_subkey(std::move(subkey))
{
}

engine::Result<std::optional<PlatformSettingValue>> WindowsSettingsStore::read(std::string_view key) const
{
    const auto wideKey = toWide(key, "The settings key");
    if (!wideKey || !validRegistryValueName(wideKey ? *wideKey : std::wstring_view{}))
    {
        return std::unexpected(wideKey ? failure(ErrorCode::InvalidArgument, "The settings key is invalid.") : wideKey.error());
    }
    const auto registry = openRegistryKey(m_subkey, KEY_QUERY_VALUE, false);
    if (!registry)
    {
        if (registry.error().code == ErrorCode::NotFound)
        {
            return std::optional<PlatformSettingValue>{};
        }
        return std::unexpected(registry.error());
    }
    DWORD type{};
    DWORD size{};
    LSTATUS status = RegQueryValueExW(registry->get(), wideKey->c_str(), nullptr, &type, nullptr, &size);
    if (status == ERROR_FILE_NOT_FOUND)
    {
        return std::optional<PlatformSettingValue>{};
    }
    if (status != ERROR_SUCCESS)
    {
        return std::unexpected(failure(ErrorCode::Io, "Unable to read the Windows setting.", static_cast<int>(status)));
    }
    if (type != REG_BINARY)
    {
        return std::unexpected(failure(ErrorCode::InvalidFormat, "The registry setting has an unsupported value type.", static_cast<int>(type)));
    }
    std::vector<std::byte> bytes(size);
    status = RegQueryValueExW(registry->get(), wideKey->c_str(), nullptr, &type, reinterpret_cast<BYTE*>(bytes.data()), &size);
    if (status != ERROR_SUCCESS)
    {
        return std::unexpected(failure(ErrorCode::Io, "Unable to read the Windows setting.", static_cast<int>(status)));
    }
    bytes.resize(size);
    const auto decoded = decodeSetting(bytes);
    if (!decoded)
    {
        return std::unexpected(decoded.error());
    }
    return std::optional<PlatformSettingValue>{*decoded};
}

engine::Status WindowsSettingsStore::write(std::string_view key, PlatformSettingValue const& value)
{
    const auto wideKey = toWide(key, "The settings key");
    if (!wideKey || !validRegistryValueName(wideKey ? *wideKey : std::wstring_view{}))
    {
        return std::unexpected(wideKey ? failure(ErrorCode::InvalidArgument, "The settings key is invalid.") : wideKey.error());
    }
    if (std::holds_alternative<std::monostate>(value))
    {
        return remove(key);
    }
    const auto encoded = encodeSetting(value);
    if (!encoded)
    {
        return std::unexpected(encoded.error());
    }
    if (encoded->size() > std::numeric_limits<DWORD>::max())
    {
        return std::unexpected(failure(ErrorCode::NumericOverflow, "The settings value is too large."));
    }
    const auto registry = openRegistryKey(m_subkey, KEY_SET_VALUE, true);
    if (!registry)
    {
        return std::unexpected(registry.error());
    }
    const LSTATUS status = RegSetValueExW(registry->get(), wideKey->c_str(), 0, REG_BINARY,
        reinterpret_cast<BYTE const*>(encoded->data()), static_cast<DWORD>(encoded->size()));
    if (status != ERROR_SUCCESS)
    {
        return std::unexpected(failure(ErrorCode::Io, "Unable to write the Windows setting.", static_cast<int>(status)));
    }
    return {};
}

engine::Status WindowsSettingsStore::remove(std::string_view key)
{
    const auto wideKey = toWide(key, "The settings key");
    if (!wideKey || !validRegistryValueName(wideKey ? *wideKey : std::wstring_view{}))
    {
        return std::unexpected(wideKey ? failure(ErrorCode::InvalidArgument, "The settings key is invalid.") : wideKey.error());
    }
    const auto registry = openRegistryKey(m_subkey, KEY_SET_VALUE, false);
    if (!registry)
    {
        return registry.error().code == ErrorCode::NotFound ? engine::Status{} : engine::Status{std::unexpected(registry.error())};
    }
    const LSTATUS status = RegDeleteValueW(registry->get(), wideKey->c_str());
    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND)
    {
        return std::unexpected(failure(ErrorCode::Io, "Unable to remove the Windows setting.", static_cast<int>(status)));
    }
    return {};
}

engine::Result<ByteBuffer> WindowsSecureStorage::protect(ByteBuffer const& plaintext)
{
    if (plaintext.size() > std::numeric_limits<DWORD>::max())
    {
        return std::unexpected(failure(ErrorCode::NumericOverflow, "The secure payload is too large."));
    }
    DATA_BLOB input{static_cast<DWORD>(plaintext.size()), reinterpret_cast<BYTE*>(const_cast<std::byte*>(plaintext.data()))};
    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"ClassMngrWinUI", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output))
    {
        return std::unexpected(windowsFailure("Windows data protection failed."));
    }
    ByteBuffer result(reinterpret_cast<std::byte*>(output.pbData), reinterpret_cast<std::byte*>(output.pbData) + output.cbData);
    LocalFree(output.pbData);
    return result;
}

engine::Result<ByteBuffer> WindowsSecureStorage::unprotect(ByteBuffer const& protectedBytes)
{
    if (protectedBytes.empty() || protectedBytes.size() > std::numeric_limits<DWORD>::max())
    {
        return std::unexpected(failure(ErrorCode::InvalidArgument, "The protected secure payload is invalid."));
    }
    DATA_BLOB input{static_cast<DWORD>(protectedBytes.size()), reinterpret_cast<BYTE*>(const_cast<std::byte*>(protectedBytes.data()))};
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output))
    {
        return std::unexpected(windowsFailure("Windows data unprotection failed."));
    }
    ByteBuffer result(reinterpret_cast<std::byte*>(output.pbData), reinterpret_cast<std::byte*>(output.pbData) + output.cbData);
    LocalFree(output.pbData);
    return result;
}

engine::Result<std::string> WindowsFileSystem::normalizePath(std::string_view path) const { return m_fileSystem.normalizePath(path); }
engine::Result<bool> WindowsFileSystem::exists(std::string_view path) const { return m_fileSystem.exists(path); }
engine::Status WindowsFileSystem::createDirectories(std::string_view path) const { return m_fileSystem.createDirectories(path); }
engine::Result<std::string> WindowsFileSystem::readBytes(std::string_view path) const { return m_fileSystem.readBytes(path); }
engine::Status WindowsFileSystem::writeBytes(std::string_view path, std::string_view bytes, bool parents) const { return m_fileSystem.writeBytes(path, bytes, parents); }
engine::Status WindowsFileSystem::copyFile(std::string_view source, std::string_view destination, bool parents) const { return m_fileSystem.copyFile(source, destination, parents); }
engine::Status WindowsFileSystem::moveFile(std::string_view source, std::string_view destination) const { return m_fileSystem.moveFile(source, destination); }
engine::Status WindowsFileSystem::replaceFileAtomically(std::string_view temporary, std::string_view destination) const { return m_fileSystem.replaceFileAtomically(temporary, destination); }
engine::Status WindowsFileSystem::replaceDirectoryAtomically(std::string_view temporary, std::string_view destination) const { return m_fileSystem.replaceDirectoryAtomically(temporary, destination); }
engine::Status WindowsFileSystem::removeFile(std::string_view path) const { return m_fileSystem.removeFile(path); }
engine::Result<std::string> WindowsFileSystem::createTemporaryDirectory(std::string_view parent) const { return m_fileSystem.createTemporaryDirectory(parent); }
engine::Status WindowsFileSystem::removeTemporaryDirectory(std::string_view path) const { return m_fileSystem.removeTemporaryDirectory(path); }

engine::Result<std::string> WindowsClipboard::readText() noexcept
{
    try
    {
        if (!OpenClipboard(nullptr)) return std::unexpected(windowsFailure("Unable to open the Windows clipboard."));
        struct Closer { ~Closer() { CloseClipboard(); } } closer;
        HANDLE const data = GetClipboardData(CF_UNICODETEXT);
        if (!data) return std::unexpected(failure(ErrorCode::NotFound, "The Windows clipboard does not contain Unicode text.", static_cast<int>(GetLastError())));
        wchar_t const* const text = static_cast<wchar_t const*>(GlobalLock(data));
        if (!text) return std::unexpected(windowsFailure("Unable to read the Windows clipboard."));
        struct Unlocker { HANDLE handle; ~Unlocker() { GlobalUnlock(handle); } } unlocker{data};
        return toUtf8(text, "The clipboard text");
    }
    catch (...) { return std::unexpected(failure(ErrorCode::Internal, "Unable to read the Windows clipboard.")); }
}

engine::Status WindowsClipboard::writeText(std::string_view utf8Text) noexcept
{
    try
    {
        const auto text = toWide(utf8Text, "The clipboard text");
        if (!text) return std::unexpected(text.error());
        const std::size_t bytes = (text->size() + 1) * sizeof(wchar_t);
        HGLOBAL const memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (!memory) return std::unexpected(windowsFailure("Unable to allocate Windows clipboard text."));
        void* const destination = GlobalLock(memory);
        if (!destination) { GlobalFree(memory); return std::unexpected(windowsFailure("Unable to write Windows clipboard text.")); }
        std::memcpy(destination, text->c_str(), bytes);
        GlobalUnlock(memory);
        if (!OpenClipboard(nullptr)) { GlobalFree(memory); return std::unexpected(windowsFailure("Unable to open the Windows clipboard.")); }
        struct Closer { ~Closer() { CloseClipboard(); } } closer;
        if (!EmptyClipboard()) { GlobalFree(memory); return std::unexpected(windowsFailure("Unable to clear the Windows clipboard.")); }
        if (!SetClipboardData(CF_UNICODETEXT, memory)) { GlobalFree(memory); return std::unexpected(windowsFailure("Unable to set Windows clipboard text.")); }
        return {};
    }
    catch (...) { return std::unexpected(failure(ErrorCode::Internal, "Unable to write Windows clipboard text.")); }
}

engine::Status WindowsUrlLauncher::openUrl(std::string_view utf8Url) noexcept
{
    try
    {
        const auto url = toWide(utf8Url, "The URL");
        if (!url || url->empty()) return std::unexpected(url ? failure(ErrorCode::InvalidArgument, "The URL is empty.") : url.error());
        SHELLEXECUTEINFOW execute{};
        execute.cbSize = sizeof(execute);
        execute.fMask = SEE_MASK_FLAG_NO_UI;
        execute.lpVerb = L"open";
        execute.lpFile = url->c_str();
        execute.nShow = SW_SHOWNORMAL;
        if (!ShellExecuteExW(&execute)) return std::unexpected(windowsFailure("Unable to open the URL."));
        if (execute.hProcess) CloseHandle(execute.hProcess);
        return {};
    }
    catch (...) { return std::unexpected(failure(ErrorCode::Internal, "Unable to open the URL.")); }
}

engine::Result<engine::ProcessResult> WindowsProcessLauncher::launch(engine::ProcessLaunchRequest const& request)
{
    const auto command = processCommandLine(request);
    const auto directory = processWorkingDirectory(request);
    if (!command || !directory) return std::unexpected(command ? directory.error() : command.error());
    if (cancelled(request.cancellation)) return std::unexpected(failure(ErrorCode::Cancelled, "The process launch was cancelled."));

    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    HANDLE outputRead{}; HANDLE outputWrite{}; HANDLE errorRead{}; HANDLE errorWrite{};
    if (!CreatePipe(&outputRead, &outputWrite, &security, 0) || !CreatePipe(&errorRead, &errorWrite, &security, 0))
    {
        const DWORD code = GetLastError(); if (outputRead) CloseHandle(outputRead); if (outputWrite) CloseHandle(outputWrite); if (errorRead) CloseHandle(errorRead); if (errorWrite) CloseHandle(errorWrite);
        return std::unexpected(windowsFailure("Unable to create process output pipes.", code));
    }
    SetHandleInformation(outputRead, HANDLE_FLAG_INHERIT, 0); SetHandleInformation(errorRead, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESTDHANDLES; startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE); startup.hStdOutput = outputWrite; startup.hStdError = errorWrite;
    PROCESS_INFORMATION process{};
    std::wstring commandLine = command->second;
    const BOOL created = CreateProcessW(command->first.c_str(), commandLine.data(), nullptr, nullptr, TRUE, 0, nullptr,
        directory->has_value() ? directory->value().c_str() : nullptr, &startup, &process);
    CloseHandle(outputWrite); CloseHandle(errorWrite);
    if (!created) { const DWORD code = GetLastError(); CloseHandle(outputRead); CloseHandle(errorRead); return std::unexpected(windowsFailure("Unable to start the process.", code)); }
    struct ProcessCloser { PROCESS_INFORMATION process; ~ProcessCloser() { CloseHandle(process.hThread); CloseHandle(process.hProcess); } } closer{process};
    struct PipeCloser { HANDLE a; HANDLE b; ~PipeCloser() { CloseHandle(a); CloseHandle(b); } } pipes{outputRead, errorRead};
    engine::ProcessResult result;
    const ULONGLONG started = GetTickCount64();
    while (true)
    {
        drainPipe(outputRead, result.standardOutput); drainPipe(errorRead, result.standardError);
        if (cancelled(request.cancellation)) { TerminateProcess(process.hProcess, ERROR_CANCELLED); return std::unexpected(failure(ErrorCode::Cancelled, "The process launch was cancelled.")); }
        if (GetTickCount64() - started >= static_cast<ULONGLONG>(request.timeout.count())) { TerminateProcess(process.hProcess, WAIT_TIMEOUT); return std::unexpected(failure(ErrorCode::Io, "The process launch timed out.", WAIT_TIMEOUT)); }
        const DWORD wait = WaitForSingleObject(process.hProcess, 15);
        if (wait == WAIT_OBJECT_0) break;
        if (wait == WAIT_FAILED) return std::unexpected(windowsFailure("Unable to wait for the process."));
    }
    drainPipe(outputRead, result.standardOutput); drainPipe(errorRead, result.standardError);
    DWORD exitCode{};
    if (!GetExitCodeProcess(process.hProcess, &exitCode)) return std::unexpected(windowsFailure("Unable to read the process exit code."));
    result.exitCode = static_cast<int>(exitCode);
    return result;
}

engine::Status WindowsProcessLauncher::launchDetached(engine::ProcessLaunchRequest const& request)
{
    const auto command = processCommandLine(request);
    const auto directory = processWorkingDirectory(request);
    if (!command || !directory) return command ? engine::Status{std::unexpected(directory.error())} : engine::Status{std::unexpected(command.error())};
    if (cancelled(request.cancellation)) return cancelledStatus("The detached process launch was cancelled.");
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{}; std::wstring commandLine = command->second;
    if (!CreateProcessW(command->first.c_str(), commandLine.data(), nullptr, nullptr, FALSE, DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP,
            nullptr, directory->has_value() ? directory->value().c_str() : nullptr, &startup, &process))
    {
        return std::unexpected(windowsFailure("Unable to start the detached process."));
    }
    CloseHandle(process.hThread); CloseHandle(process.hProcess); return {};
}

engine::Status WindowsNotificationSink::registerForAppNotifications() noexcept
{
    try { winrt::Microsoft::Windows::AppNotifications::AppNotificationManager::Default().Register(); m_registered = true; return {}; }
    catch (winrt::hresult_error const& error) { return std::unexpected(failure(ErrorCode::Unsupported, "Windows App Notifications are unavailable.", static_cast<int>(error.code()))); }
    catch (...) { return std::unexpected(failure(ErrorCode::Internal, "Unable to register Windows App Notifications.")); }
}

engine::Status WindowsNotificationSink::show(std::string_view xmlPayload) noexcept
{
    try
    {
        if (!m_registered) return std::unexpected(failure(ErrorCode::Unsupported, "Windows App Notifications have not been registered."));
        const auto xml = toWide(xmlPayload, "The notification payload");
        if (!xml || xml->empty()) return std::unexpected(xml ? failure(ErrorCode::InvalidArgument, "The notification payload is empty.") : xml.error());
        winrt::Microsoft::Windows::AppNotifications::AppNotificationManager::Default().Show(
            winrt::Microsoft::Windows::AppNotifications::AppNotification{*xml}
            );
        return {};
    }
    catch (winrt::hresult_error const& error) { return std::unexpected(failure(ErrorCode::Unsupported, "Windows App Notification delivery failed.", static_cast<int>(error.code()))); }
    catch (...) { return std::unexpected(failure(ErrorCode::Internal, "Windows App Notification delivery failed.")); }
}

WindowsLogger::WindowsLogger(std::wstring utf16LogFile)
    : m_logFile(utf16LogFile.empty() ? defaultLogFile() : std::move(utf16LogFile)) {}

void WindowsLogger::log(engine::LogRecord const& record) noexcept
{
    try
    {
        std::wstring line = L"[" + severityName(record.severity) + L"] ";
        const auto message = toWide(record.message, "The log message");
        line += message ? *message : L"<invalid UTF-8 log message>";
        for (auto const& [key, value] : record.context)
        {
            const auto wideKey = toWide(key, "The log context key"); const auto wideValue = toWide(value, "The log context value");
            line += L" "; line += wideKey ? *wideKey : L"<invalid>"; line += L"="; line += wideValue ? *wideValue : L"<invalid>";
        }
        line += L"\r\n"; OutputDebugStringW(line.c_str());
        if (m_logFile.empty()) return;
        HANDLE const file = CreateFileW(m_logFile.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return;
        const auto utf8 = toUtf8(line, "The log line");
        if (utf8 && utf8->size() <= std::numeric_limits<DWORD>::max()) { DWORD written{}; WriteFile(file, utf8->data(), static_cast<DWORD>(utf8->size()), &written, nullptr); }
        CloseHandle(file);
    }
    catch (...) { }
}

engine::Status WindowsCrashDiagnostics::install() noexcept
{
    try { SetUnhandledExceptionFilter(unhandledExceptionFilter); return {}; }
    catch (...) { return std::unexpected(failure(ErrorCode::Internal, "Unable to install Windows crash diagnostics.")); }
}

bool runPlatformAdapterContractChecks() noexcept
{
    try
    {
        const PlatformSettingValue expected{std::string("UTF-8 ì¤ì ")};
        const auto encoded = encodeSetting(expected); if (!encoded) return false;
        const auto decoded = decodeSetting(*encoded); if (!decoded || decoded->index() != expected.index() || std::get<std::string>(*decoded) != std::get<std::string>(expected)) return false;
        const auto bytes = encodeSetting(PlatformSettingValue{ByteBuffer{std::byte{0}, std::byte{0xff}}}); if (!bytes) return false;
        const auto decodedBytes = decodeSetting(*bytes); if (!decodedBytes || std::get<ByteBuffer>(*decodedBytes).size() != 2) return false;
        const std::array<std::byte, settingHeaderSize> malformed{}; return !decodeSetting({malformed.begin(), malformed.end()}) && !toWide("\xff", "invalid UTF-8");
    }
    catch (...) { return false; }
}

} // namespace classmngr::windows::winui
