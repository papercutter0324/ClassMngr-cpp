#include "pch.h"

#include "winui_lifecycle.h"
#include "winui_identity.h"

#include <shellapi.h>
#include <shlobj_core.h>

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

constexpr std::wstring_view lifecycleTestSwitch = L"--phase3-lifecycle-test";
constexpr std::wstring_view fatalFileName = L"fatal-error.log";

bool isOption(std::wstring_view argument) noexcept
{
    return !argument.empty() && (argument.front() == L'-' || argument.front() == L'/');
}

std::wstring utf8(std::wstring_view value)
{
    return std::wstring(value);
}

std::string toUtf8(std::wstring_view value)
{
    if (value.empty())
    {
        return {};
    }

    const int length = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr
        );
    if (length <= 0)
    {
        return {};
    }

    std::string result(static_cast<std::size_t>(length), '\0');
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            length,
            nullptr,
            nullptr
            ) <= 0)
    {
        return {};
    }
    return result;
}

void createDirectoryIfNeeded(std::wstring const& path) noexcept
{
    if (!CreateDirectoryW(path.c_str(), nullptr))
    {
        static_cast<void>(GetLastError());
    }
}

std::wstring fatalReportPath() noexcept
{
    PWSTR localAppData{};
    if (FAILED(SHGetKnownFolderPath(
            FOLDERID_LocalAppData,
            KF_FLAG_CREATE,
            nullptr,
            &localAppData
            )))
    {
        return {};
    }

    std::wstring root(localAppData);
    CoTaskMemFree(localAppData);
    root += L"\\PaperCloud";
    createDirectoryIfNeeded(root);
    root += L"\\ClassMngrWinUI";
    createDirectoryIfNeeded(root);
    root += L"\\Logs";
    createDirectoryIfNeeded(root);
    root += L"\\";
    root += fatalFileName;
    return root;
}

} // namespace

namespace ClassMngrWinUILifecycle
{

CommandLineActivation parseArguments(
    std::span<std::wstring_view const> arguments
    )
{
    CommandLineActivation activation;
    activation.arguments.reserve(arguments.size());
    for (std::wstring_view argument : arguments)
    {
        activation.arguments.emplace_back(argument);
        if (!isOption(argument))
        {
            activation.openTargets.emplace_back(argument);
        }
        if (argument == lifecycleTestSwitch)
        {
            activation.lifecycleTest = true;
        }
    }
    return activation;
}

CommandLineActivation parseWindowsCommandLine(std::wstring_view commandLine)
{
    std::wstring mutableCommandLine(commandLine);
    int argumentCount{};
    LPWSTR* argv = CommandLineToArgvW(
        mutableCommandLine.data(),
        &argumentCount
        );
    if (!argv || argumentCount <= 1)
    {
        if (argv)
        {
            LocalFree(argv);
        }
        return {};
    }

    std::vector<std::wstring_view> arguments;
    arguments.reserve(static_cast<std::size_t>(argumentCount - 1));
    for (int index = 1; index < argumentCount; ++index)
    {
        arguments.emplace_back(argv[index]);
    }
    CommandLineActivation activation = parseArguments(arguments);
    LocalFree(argv);
    return activation;
}

CommandLineActivation parseCommandLineActivationArguments(
    std::wstring_view arguments
    )
{
    std::wstring commandLine = L"ClassMngrWinUI.exe";
    if (!arguments.empty())
    {
        commandLine += L" ";
        commandLine += arguments;
    }
    return parseWindowsCommandLine(commandLine);
}

bool hasArgument(
    CommandLineActivation const& activation,
    std::wstring_view argument
    )
{
    for (std::wstring const& candidate : activation.arguments)
    {
        if (candidate == argument)
        {
            return true;
        }
    }
    return false;
}

InstanceDisposition instanceDisposition(bool isCurrent) noexcept
{
    return isCurrent
        ? InstanceDisposition::Primary
        : InstanceDisposition::Secondary;
}

bool runLifecycleContractChecks() noexcept
{
    constexpr std::array<std::wstring_view, 3> arguments{
        L"--phase3-lifecycle-test",
        L"C:\\Class Mngr\\open file.classmngr",
        L"--unrelated-switch",
    };
    const CommandLineActivation activation = parseArguments(arguments);
    return activation.lifecycleTest
        && activation.arguments.size() == arguments.size()
        && activation.openTargets.size() == 1
        && activation.openTargets.front() == arguments[1]
        && instanceDisposition(true) == InstanceDisposition::Primary
        && instanceDisposition(false) == InstanceDisposition::Secondary
        && std::wstring_view(ClassMngrWinUIIdentity::SingleInstanceKey)
            != ClassMngrWinUIIdentity::AppUserModelId;
}

void reportFatalError(std::wstring_view message) noexcept
{
    const std::wstring report =
        L"ClassMngr WinUI fatal error\r\n" + utf8(message) + L"\r\n";
    OutputDebugStringW(report.c_str());

    const std::wstring path = fatalReportPath();
    if (path.empty())
    {
        return;
    }

    const std::string utf8Report = toUtf8(report);
    HANDLE const file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
        );
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }

    DWORD bytesWritten{};
    WriteFile(
        file,
        utf8Report.data(),
        static_cast<DWORD>(utf8Report.size()),
        &bytesWritten,
        nullptr
        );
    CloseHandle(file);
}

} // namespace ClassMngrWinUILifecycle
