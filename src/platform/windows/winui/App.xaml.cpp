#include "pch.h"

#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include "winui_lifecycle.h"
#include "winui_identity.h"
#include "winui_platform_services.h"
#include "winui_threading.h"

#include <shellapi.h>
#include <shobjidl_core.h>

#include <string>
#include <string_view>

#include <winrt/Windows.ApplicationModel.Activation.h>

#if __has_include("App.g.cpp")
#include "App.g.cpp"
#endif

namespace
{

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
    winrt::Microsoft::UI::Xaml::Window const& window,
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

winrt::fire_and_forget completeViewModelTest(
    winrt::Microsoft::UI::Xaml::Window const& window,
    winrt::Windows::Foundation::IAsyncOperation<bool> check
    )
{
    bool passed{};
    try
    {
        passed = co_await check;
    }
    catch (...)
    {
        passed = false;
    }
    scheduleTestExit(window, passed);
}

} // namespace

namespace winrt::ClassMngrWinUI::implementation
{

App::App()
{
    InitializeComponent();
    static_cast<void>(
        classmngr::windows::winui::WindowsCrashDiagnostics::install()
        );
    SetCurrentProcessExplicitAppUserModelID(
        ClassMngrWinUIIdentity::AppUserModelId
        );
    const auto application = Microsoft::UI::Xaml::Application::Current();
    m_suspendingToken =
        Windows::ApplicationModel::Core::CoreApplication::Suspending(
            {this, &App::OnSuspending}
            );
    m_resumingToken =
        Windows::ApplicationModel::Core::CoreApplication::Resuming(
            {this, &App::OnResuming}
            );
    m_unhandledExceptionToken = application.UnhandledException(
        {this, &App::OnUnhandledException}
        );
}

App::~App()
{
    closeLifecycle();
}

void App::OnLaunched(
    Microsoft::UI::Xaml::LaunchActivatedEventArgs const& arguments
    )
{
    static_cast<void>(arguments);

    const auto activation = ClassMngrWinUILifecycle::parseWindowsCommandLine(
        GetCommandLineW()
        );
    if (ClassMngrWinUILifecycle::hasArgument(
            activation,
            L"--phase1-manifest-test"
            ))
    {
        ExitProcess(
            verifyEmbeddedManifest()
                ? ERROR_SUCCESS
                : ERROR_BAD_EXE_FORMAT
            );
        return;
    }

    if (activation.lifecycleTest)
    {
        ExitProcess(
            ClassMngrWinUILifecycle::runLifecycleContractChecks()
                ? ERROR_SUCCESS
                : ERROR_INVALID_DATA
            );
        return;
    }

    try
    {
        if (!initializeSingleInstance())
        {
            Microsoft::UI::Xaml::Application::Current().Exit();
            return;
        }
    }
    catch (winrt::hresult_error const& error)
    {
        ClassMngrWinUILifecycle::reportFatalError(
            L"Single-instance activation failed: "
                + std::wstring(error.message())
            );
        terminateAfterFatalError();
        return;
    }
    catch (...)
    {
        ClassMngrWinUILifecycle::reportFatalError(
            L"Single-instance activation failed."
            );
        terminateAfterFatalError();
        return;
    }

    m_window = winrt::make<MainWindow>();
    m_window.Title(ClassMngrWinUIIdentity::WindowTitle);
    m_dispatcherQueue = m_window.DispatcherQueue();
    m_window.Activate();

    const bool smokeTest = ClassMngrWinUILifecycle::hasArgument(
        activation,
        L"--phase1-smoke-test"
        );
    const bool inputTest = ClassMngrWinUILifecycle::hasArgument(
        activation,
        L"--phase1-input-test"
        );
    const bool themeTest = ClassMngrWinUILifecycle::hasArgument(
        activation,
        L"--phase1-theme-test"
        );
    const bool dpiTest = ClassMngrWinUILifecycle::hasArgument(
        activation,
        L"--phase1-dpi-test"
        );
    const bool navigationTest = ClassMngrWinUILifecycle::hasArgument(
        activation,
        L"--phase3-navigation-test"
        );
    const bool viewModelTest = ClassMngrWinUILifecycle::hasArgument(
        activation,
        L"--phase3-view-model-test"
        );
    const bool localizationTest = ClassMngrWinUILifecycle::hasArgument(
        activation,
        L"--phase3-localization-test"
        );
    const bool dialogTest = ClassMngrWinUILifecycle::hasArgument(
        activation,
        L"--phase3-dialog-test"
        );
    const bool threadingTest = ClassMngrWinUILifecycle::hasArgument(
        activation,
        L"--phase3-threading-test"
        );
    if (smokeTest || inputTest || themeTest || dpiTest || navigationTest
        || viewModelTest || localizationTest || dialogTest || threadingTest)
    {
        const auto runChecks = [this, smokeTest, inputTest, themeTest, dpiTest, navigationTest, viewModelTest, localizationTest, dialogTest, threadingTest]() {
            auto* mainWindow = winrt::get_self<MainWindow>(
                m_window.as<::winrt::ClassMngrWinUI::MainWindow>()
                );
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
            else if (dpiTest)
            {
                passed = mainWindow->runPhase1DpiChecks();
            }
            else if (navigationTest)
            {
                passed = mainWindow->runPhase3NavigationChecks();
            }
            else if (viewModelTest)
            {
                completeViewModelTest(
                    m_window,
                    mainWindow->runPhase3ViewModelChecks()
                    );
                return;
            }
            else if (localizationTest)
            {
                passed = mainWindow->runPhase3LocalizationChecks();
            }
            else if (dialogTest)
            {
                passed = mainWindow->runPhase3DialogChecks();
            }
            else if (threadingTest)
            {
                passed = ClassMngrWinUIThreading::runThreadingContractChecks();
            }
            scheduleTestExit(m_window, passed);
        };

        const bool queued = dpiTest
            ? m_window.DispatcherQueue().TryEnqueue(
                Microsoft::UI::Dispatching::DispatcherQueuePriority::Low,
                runChecks
                )
            : m_window.DispatcherQueue().TryEnqueue(
                runChecks
                );
        if (!queued)
        {
            ExitProcess(ERROR_INVALID_DATA);
        }
    }
}

bool App::initializeSingleInstance()
{
    using winrt::Microsoft::Windows::AppLifecycle::AppInstance;

    const AppInstance currentInstance = AppInstance::GetCurrent();
    const auto activationArguments = currentInstance.GetActivatedEventArgs();
    const AppInstance primaryInstance = AppInstance::FindOrRegisterForKey(
        ClassMngrWinUIIdentity::SingleInstanceKey
        );
    if (!primaryInstance.IsCurrent())
    {
        primaryInstance.RedirectActivationToAsync(activationArguments).get();
        return false;
    }

    m_instance = primaryInstance;
    auto weak = get_weak();
    m_activationToken = m_instance.Activated(
        [weak](Windows::Foundation::IInspectable const&,
               Microsoft::Windows::AppLifecycle::AppActivationArguments const&
                   activationArguments) {
            if (auto app = weak.get())
            {
                app->OnAppInstanceActivated(activationArguments);
            }
        }
        );
    return true;
}

void App::OnAppInstanceActivated(
    Microsoft::Windows::AppLifecycle::AppActivationArguments const& arguments
    )
{
    auto weak = get_weak();
    if (!m_dispatcherQueue
        || !m_dispatcherQueue.TryEnqueue(
            [weak, arguments]() {
                if (auto app = weak.get())
                {
                    app->handleActivation(arguments);
                }
            }
            ))
    {
        ClassMngrWinUILifecycle::reportFatalError(
            L"A redirected activation could not be dispatched to the UI thread."
            );
    }
}

void App::handleActivation(
    Microsoft::Windows::AppLifecycle::AppActivationArguments const& arguments
    )
{
    if (arguments.Kind()
        == Microsoft::Windows::AppLifecycle::ExtendedActivationKind::CommandLineLaunch)
    {
        const auto commandLineArguments = arguments.Data().try_as<
            Windows::ApplicationModel::Activation::CommandLineActivatedEventArgs>();
        if (commandLineArguments)
        {
            static_cast<void>(
                ClassMngrWinUILifecycle::parseCommandLineActivationArguments(
                    commandLineArguments.Operation().Arguments().c_str()
                    )
                );
        }
    }

    if (m_window)
    {
        m_window.Activate();
    }
}

void App::OnSuspending(
    Windows::Foundation::IInspectable const& sender,
    Windows::ApplicationModel::SuspendingEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    m_isSuspended = true;
}

void App::OnResuming(
    Windows::Foundation::IInspectable const& sender,
    Windows::Foundation::IInspectable const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    m_isSuspended = false;
}

void App::OnUnhandledException(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::UnhandledExceptionEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    arguments.Handled(true);
    ClassMngrWinUILifecycle::reportFatalError(
        L"Unhandled XAML exception: " + std::wstring(arguments.Message())
        );
    terminateAfterFatalError();
}

void App::closeLifecycle() noexcept
{
    try
    {
        if (m_instance && m_activationToken.value != 0)
        {
            m_instance.Activated(m_activationToken);
            m_activationToken = {};
        }
        if (m_suspendingToken.value != 0)
        {
            Windows::ApplicationModel::Core::CoreApplication::Suspending(
                m_suspendingToken
                );
            m_suspendingToken = {};
        }
        if (m_resumingToken.value != 0)
        {
            Windows::ApplicationModel::Core::CoreApplication::Resuming(
                m_resumingToken
                );
            m_resumingToken = {};
        }
        if (m_unhandledExceptionToken.value != 0)
        {
            Microsoft::UI::Xaml::Application::Current().as<
                Microsoft::UI::Xaml::IApplication>().UnhandledException(
                m_unhandledExceptionToken
                );
            m_unhandledExceptionToken = {};
        }
        if (m_instance)
        {
            m_instance.UnregisterKey();
            m_instance = nullptr;
        }
        m_dispatcherQueue = nullptr;
        m_window = nullptr;
    }
    catch (...)
    {
        // Shutdown must not let a failed cleanup escape an App destructor.
    }
}

void App::terminateAfterFatalError() noexcept
{
    closeLifecycle();
    try
    {
        Microsoft::UI::Xaml::Application::Current().Exit();
    }
    catch (...)
    {
        // The report is already durable; there is no safe UI recovery path.
    }
}

} // namespace winrt::ClassMngrWinUI::implementation
