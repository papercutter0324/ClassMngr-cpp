#include "pch.h"

#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include "winui_identity.h"

#include <shellapi.h>

#include <string>
#include <string_view>

#if __has_include("App.g.cpp")
#include "App.g.cpp"
#endif

namespace
{

bool commandLineContains(std::wstring_view value)
{
    const wchar_t* commandLine = GetCommandLineW();
    return commandLine != nullptr
        && std::wstring_view(commandLine).find(value)
            != std::wstring_view::npos;
}

std::wstring embeddedManifest()
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    HRSRC resource = FindResourceW(
        instance,
        MAKEINTRESOURCEW(1),
        RT_MANIFEST
        );
    if (!resource)
    {
        return {};
    }

    HGLOBAL loadedResource = LoadResource(instance, resource);
    if (!loadedResource)
    {
        return {};
    }

    const DWORD size = SizeofResource(instance, resource);
    const auto* bytes = static_cast<const char*>(LockResource(loadedResource));
    if (!bytes || size == 0)
    {
        return {};
    }

    const int utf16Length = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        bytes,
        static_cast<int>(size),
        nullptr,
        0
        );
    if (utf16Length > 0)
    {
        std::wstring manifest(static_cast<std::size_t>(utf16Length), L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            bytes,
            static_cast<int>(size),
            manifest.data(),
            utf16Length
            );
        return manifest;
    }

    return std::wstring(
        reinterpret_cast<const wchar_t*>(bytes),
        reinterpret_cast<const wchar_t*>(bytes)
            + (size / sizeof(wchar_t))
        );
}

bool verifyEmbeddedManifest()
{
    const std::wstring manifest = embeddedManifest();
    const auto contains = [&manifest](std::wstring_view value) {
        return manifest.find(value) != std::wstring::npos;
    };

    return contains(L"{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}")
        && contains(L"processorArchitecture=\"*\"")
        && contains(L"PerMonitorV2, PerMonitor, System")
        && contains(L"longPathAware>true")
        && contains(L"requestedExecutionLevel level=\"asInvoker\"");
}

void scheduleTestExit(
    Microsoft::UI::Xaml::Window const& window,
    bool passed
    )
{
    window.DispatcherQueue().TryEnqueue(
        [window, passed]() {
            window.Close();
            ExitProcess(passed ? ERROR_SUCCESS : ERROR_INVALID_DATA);
        }
        );
}

} // namespace

namespace winrt::ClassMngrWinUI::implementation
{

App::App()
{
    InitializeComponent();
    SetCurrentProcessExplicitAppUserModelID(
        ClassMngrWinUIIdentity::AppUserModelId
        );
}

void App::OnLaunched(
    Microsoft::UI::Xaml::LaunchActivatedEventArgs const& arguments
    )
{
    static_cast<void>(arguments);

    if (commandLineContains(L"--phase1-manifest-test"))
    {
        ExitProcess(
            verifyEmbeddedManifest()
                ? ERROR_SUCCESS
                : ERROR_BAD_EXE_FORMAT
            );
        return;
    }

    m_window = winrt::make<MainWindow>();
    m_window.Title(ClassMngrWinUIIdentity::WindowTitle);
    m_window.Activate();

    const bool smokeTest = commandLineContains(L"--phase1-smoke-test");
    const bool inputTest = commandLineContains(L"--phase1-input-test");
    const bool themeTest = commandLineContains(L"--phase1-theme-test");
    if (smokeTest || inputTest || themeTest)
    {
        const bool queued = m_window.DispatcherQueue().TryEnqueue(
            [this, smokeTest, inputTest, themeTest]() {
                auto* mainWindow = winrt::get_self<MainWindow>(m_window);
                bool passed = true;
                if (smokeTest)
                {
                    passed = mainWindow->runPhase1SmokeChecks();
                }
                else if (inputTest)
                {
                    passed = mainWindow->runPhase1InputChecks();
                }
                else if (themeTest)
                {
                    passed = mainWindow->runPhase1ThemeChecks();
                }
                scheduleTestExit(m_window, passed);
            }
            );
        if (!queued)
        {
            ExitProcess(ERROR_INVALID_DATA);
        }
    }
}

} // namespace winrt::ClassMngrWinUI::implementation
