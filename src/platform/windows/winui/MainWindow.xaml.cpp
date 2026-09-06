#include "pch.h"

#include "MainWindow.xaml.h"
#include "winui_build_info.h"
#include "winui_identity.h"
#include "winui_shared_ux.h"

#include <microsoft.ui.xaml.window.h>

#include <winrt/Microsoft.UI.Xaml.Automation.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <coroutine>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

namespace
{

constexpr std::wstring_view homePageId = L"home";
constexpr std::wstring_view aboutPageId = L"about";

struct ResumeOnDispatcherQueue
{
    winrt::Microsoft::UI::Dispatching::DispatcherQueue dispatcher;
    winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority priority;

    [[nodiscard]] bool await_ready() const noexcept
    {
        return false;
    }

    void await_suspend(std::coroutine_handle<> continuation) const
    {
        if (!dispatcher.TryEnqueue(
                priority,
                [continuation]() noexcept {
                    continuation.resume();
                }
                ))
        {
            throw winrt::hresult_illegal_method_call();
        }
    }

    void await_resume() const noexcept
    {
    }
};

struct PersistedShellState
{
    std::wstring selectedPage{std::wstring(homePageId)};
    std::wstring navigationState;
    RECT windowBounds{};
    bool hasWindowBounds{};
};

bool isKnownPageId(std::wstring_view pageId) noexcept
{
    return pageId == homePageId || pageId == aboutPageId;
}

std::wstring asWString(winrt::hstring const& value)
{
    return std::wstring(value.c_str(), value.size());
}

std::wstring boxedString(
    winrt::Windows::Foundation::IInspectable const& value
    )
{
    if (!value)
    {
        return {};
    }

    try
    {
        return asWString(winrt::unbox_value<winrt::hstring>(value));
    }
    catch (...)
    {
        return {};
    }
}

winrt::Windows::Foundation::IAsyncAction phase3PresentationWork(
    classmngr::engine::CancellationToken const& cancellation
    )
{
    using namespace std::chrono_literals;

    co_await winrt::resume_after(250ms);
    if (cancellation.isCancellationRequested())
    {
        co_return;
    }

    co_await winrt::resume_after(250ms);
}

bool readRegistryString(
    HKEY key,
    wchar_t const* valueName,
    std::wstring& value
    ) noexcept
{
    DWORD type{};
    DWORD byteCount{};
    if (RegQueryValueExW(
            key,
            valueName,
            nullptr,
            &type,
            nullptr,
            &byteCount
            ) != ERROR_SUCCESS
        || type != REG_SZ
        || byteCount < sizeof(wchar_t))
    {
        return false;
    }

    std::wstring result(byteCount / sizeof(wchar_t), L'\0');
    if (RegQueryValueExW(
            key,
            valueName,
            nullptr,
            &type,
            reinterpret_cast<LPBYTE>(result.data()),
            &byteCount
            ) != ERROR_SUCCESS)
    {
        return false;
    }

    if (!result.empty() && result.back() == L'\0')
    {
        result.pop_back();
    }
    value = std::move(result);
    return true;
}

bool readRegistryDword(
    HKEY key,
    wchar_t const* valueName,
    DWORD& value
    ) noexcept
{
    DWORD type{};
    DWORD byteCount = sizeof(value);
    return RegQueryValueExW(
               key,
               valueName,
               nullptr,
               &type,
               reinterpret_cast<LPBYTE>(&value),
               &byteCount
               ) == ERROR_SUCCESS
        && type == REG_DWORD
        && byteCount == sizeof(value);
}

void writeRegistryString(
    HKEY key,
    wchar_t const* valueName,
    std::wstring const& value
    ) noexcept
{
    const DWORD byteCount = static_cast<DWORD>(
        (value.size() + 1) * sizeof(wchar_t)
        );
    RegSetValueExW(
        key,
        valueName,
        0,
        REG_SZ,
        reinterpret_cast<BYTE const*>(value.c_str()),
        byteCount
        );
}

void writeRegistryDword(
    HKEY key,
    wchar_t const* valueName,
    DWORD value
    ) noexcept
{
    RegSetValueExW(
        key,
        valueName,
        0,
        REG_DWORD,
        reinterpret_cast<BYTE const*>(&value),
        sizeof(value)
        );
}

PersistedShellState loadShellState() noexcept
{
    PersistedShellState state;
    HKEY key{};
    if (RegOpenKeyExW(
            HKEY_CURRENT_USER,
            ClassMngrWinUIIdentity::ShellStateRegistrySubkey,
            0,
            KEY_READ,
            &key
            ) != ERROR_SUCCESS)
    {
        return state;
    }

    std::wstring selectedPage;
    if (readRegistryString(key, L"SelectedPage", selectedPage)
        && isKnownPageId(selectedPage))
    {
        state.selectedPage = std::move(selectedPage);
    }
    readRegistryString(key, L"NavigationState", state.navigationState);

    DWORD value{};
    const bool hasLeft = readRegistryDword(key, L"WindowLeft", value);
    if (hasLeft)
    {
        state.windowBounds.left = static_cast<LONG>(value);
    }
    const bool hasTop = readRegistryDword(key, L"WindowTop", value);
    if (hasTop)
    {
        state.windowBounds.top = static_cast<LONG>(value);
    }
    const bool hasRight = readRegistryDword(key, L"WindowRight", value);
    if (hasRight)
    {
        state.windowBounds.right = static_cast<LONG>(value);
    }
    const bool hasBottom = readRegistryDword(key, L"WindowBottom", value);
    if (hasBottom)
    {
        state.windowBounds.bottom = static_cast<LONG>(value);
    }
    state.hasWindowBounds =
        hasLeft && hasTop && hasRight && hasBottom;

    RegCloseKey(key);
    return state;
}

HKEY openShellStateForWrite() noexcept
{
    HKEY key{};
    if (RegCreateKeyExW(
            HKEY_CURRENT_USER,
            ClassMngrWinUIIdentity::ShellStateRegistrySubkey,
            0,
            nullptr,
            REG_OPTION_NON_VOLATILE,
            KEY_WRITE,
            nullptr,
            &key,
            nullptr
            ) != ERROR_SUCCESS)
    {
        return nullptr;
    }
    return key;
}

HWND windowHandle(
    winrt::ClassMngrWinUI::implementation::MainWindow* window
    ) noexcept
{
    try
    {
        const auto inspectable = static_cast<
            winrt::Windows::Foundation::IInspectable>(*window);
        winrt::com_ptr<::IWindowNative> nativeWindow;
        if (inspectable
            && SUCCEEDED(winrt::get_unknown(inspectable)->QueryInterface(
                __uuidof(::IWindowNative),
                nativeWindow.put_void())))
        {
            HWND handle{};
            if (SUCCEEDED(nativeWindow->get_WindowHandle(&handle)))
            {
                return handle;
            }
        }
    }
    catch (...)
    {
    }
    return nullptr;
}

bool isUsableWindowBounds(RECT const& bounds) noexcept
{
    const LONG width = bounds.right - bounds.left;
    const LONG height = bounds.bottom - bounds.top;
    return width >= 640
        && width <= 10000
        && height >= 420
        && height <= 10000
        && bounds.left > -100000
        && bounds.left < 100000
        && bounds.top > -100000
        && bounds.top < 100000;
}

void setAutomationName(
    winrt::Microsoft::UI::Xaml::DependencyObject const& element,
    std::wstring_view name
    )
{
    winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
        element,
        winrt::hstring(name)
    );
}

std::vector<std::wstring> splitPastedRangeRow(std::wstring_view row)
{
    std::vector<std::wstring> values;
    size_t start = 0;
    while (start <= row.size())
    {
        const size_t separator = row.find(L'\t', start);
        const size_t end = separator == std::wstring_view::npos
            ? row.size()
            : separator;
        values.emplace_back(row.substr(start, end - start));
        if (separator == std::wstring_view::npos)
        {
            break;
        }
        start = separator + 1;
    }
    return values;
}

std::vector<std::vector<std::wstring>> parsePastedRange(
    std::wstring_view text
    )
{
    std::vector<std::vector<std::wstring>> rows;
    size_t start = 0;
    while (start <= text.size())
    {
        const size_t separator = text.find(L'\n', start);
        const size_t end = separator == std::wstring_view::npos
            ? text.size()
            : separator;
        auto row = text.substr(start, end - start);
        if (!row.empty() && row.back() == L'\r')
        {
            row.remove_suffix(1);
        }
        if (!row.empty())
        {
            rows.emplace_back(splitPastedRangeRow(row));
        }
        if (separator == std::wstring_view::npos)
        {
            break;
        }
        start = separator + 1;
    }
    return rows;
}

} // namespace

namespace winrt::ClassMngrWinUI::implementation
{

MainWindow::MainWindow()
{
    InitializeComponent();

    const auto parsedVersion = classmngr::engine::SemanticVersion::parse(
        ::ClassMngrWinUI::BuildInfo::Version
        );
    if (parsedVersion)
    {
        m_engineVersion = *parsedVersion;
    }

    m_appTitleBar = RootGrid().FindName(L"AppTitleBar").as<
        Microsoft::UI::Xaml::Controls::Grid>();
    m_navigationView = RootGrid().FindName(L"RootNavigationView").as<
        Microsoft::UI::Xaml::Controls::NavigationView>();
    m_homeNavigationItem = RootGrid().FindName(L"HomeNavigationItem").as<
        Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_aboutNavigationItem = RootGrid().FindName(L"AboutNavigationItem").as<
        Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_contentFrame = RootGrid().FindName(L"ContentFrame").as<
        Microsoft::UI::Xaml::Controls::Frame>();
    m_shellInfoButton = RootGrid().FindName(L"ShellInfoButton").as<
        Microsoft::UI::Xaml::Controls::Button>();

    m_aboutNavigationItem.Content(winrt::box_value(winrt::hstring(
        m_localizer.getString(L"ActionRegistry", L"About")
        )));
    m_shellInfoButton.Content(winrt::box_value(winrt::hstring(
        m_localizer.getString(
            L"ActionRegistry",
            L"Show application information"
            )
        )));

    ExtendsContentIntoTitleBar(true);
    SetTitleBar(m_appTitleBar);
    m_contentFrame.CacheSize(2);
    m_contentFrame.IsNavigationStackEnabled(true);

    m_selectionChangedToken = m_navigationView.SelectionChanged(
        {this, &MainWindow::NavigationView_SelectionChanged}
        );
    m_backRequestedToken = m_navigationView.BackRequested(
        {this, &MainWindow::NavigationView_BackRequested}
        );
    m_navigatedToken = m_contentFrame.Navigated(
        {this, &MainWindow::ContentFrame_Navigated}
        );
    m_activatedToken = Activated({this, &MainWindow::Window_Activated});
    m_closedToken = Closed({this, &MainWindow::Window_Closed});

    restoreShellState();
}

MainWindow::~MainWindow()
{
    closeShell();
}

bool MainWindow::runPhase1SmokeChecks()
{
    return m_engineVersion.isValid()
        && static_cast<bool>(RootGrid())
        && static_cast<bool>(m_navigationView)
        && static_cast<bool>(m_contentFrame)
        && static_cast<bool>(m_engineVersionText)
        && static_cast<bool>(m_nameTextBox)
        && static_cast<bool>(m_continueButton)
        && static_cast<bool>(m_statusText)
        && m_contentFrame.Content()
        && m_currentPageId == homePageId
        && m_engineVersionText.Text()
            == winrt::to_hstring(
                std::string("Engine version: ")
                    + m_engineVersion.toString()
                );
}

bool MainWindow::runPhase1InputChecks()
{
    if (!ensureHomePage())
    {
        return false;
    }

    m_nameTextBox.Text(L"한글 입력");
    m_nameTextBox.Focus(
        Microsoft::UI::Xaml::FocusState::Programmatic
        );

    const auto inputScope = m_nameTextBox.InputScope();
    if (!inputScope)
    {
        return false;
    }

    bool hasTextInputScope = false;
    const auto inputScopeNames = inputScope.Names();
    for (uint32_t index = 0; index < inputScopeNames.Size(); ++index)
    {
        if (inputScopeNames.GetAt(index).NameValue()
            == Microsoft::UI::Xaml::Input::InputScopeNameValue::Text)
        {
            hasTextInputScope = true;
            break;
        }
    }

    return m_nameTextBox.Text() == winrt::hstring(L"한글 입력")
        && hasTextInputScope
        && m_nameTextBox.IsTabStop()
        && m_nameTextBox.TabIndex() == 0
        && m_continueButton.IsTabStop()
        && m_continueButton.TabIndex() == 1;
}

bool MainWindow::runPhase1ThemeChecks()
{
    RootGrid().RequestedTheme(Microsoft::UI::Xaml::ElementTheme::Light);
    const bool lightTheme = RootGrid().ActualTheme()
        == Microsoft::UI::Xaml::ElementTheme::Light;

    RootGrid().RequestedTheme(Microsoft::UI::Xaml::ElementTheme::Dark);
    const bool darkTheme = RootGrid().ActualTheme()
        == Microsoft::UI::Xaml::ElementTheme::Dark;

    RootGrid().RequestedTheme(Microsoft::UI::Xaml::ElementTheme::Default);
    return lightTheme && darkTheme;
}

bool MainWindow::runPhase1DpiChecks()
{
    const auto xamlRoot = RootGrid().XamlRoot();
    return static_cast<bool>(xamlRoot)
        && xamlRoot.RasterizationScale() > 0.0
        && RootGrid().ActualWidth() > 0.0
        && RootGrid().ActualHeight() > 0.0
        && m_contentFrame.ActualWidth() > 0.0
        && m_contentFrame.ActualHeight() > 0.0;
}

bool MainWindow::runPhase3NavigationChecks()
{
    const bool shellReady = static_cast<bool>(m_navigationView)
        && static_cast<bool>(m_homeNavigationItem)
        && static_cast<bool>(m_aboutNavigationItem)
        && static_cast<bool>(m_contentFrame)
        && m_contentFrame.IsNavigationStackEnabled()
        && m_contentFrame.CacheSize() >= 2
        && static_cast<bool>(m_shellInfoButton);
    if (!shellReady)
    {
        return false;
    }

    navigateTo(homePageId);
    const bool homeReady = m_currentPageId == homePageId
        && static_cast<bool>(m_contentFrame.Content());

    navigateTo(aboutPageId);
    const bool aboutReady = m_currentPageId == aboutPageId
        && static_cast<bool>(m_contentFrame.Content())
        && m_contentFrame.CanGoBack();

    if (m_contentFrame.CanGoBack())
    {
        m_contentFrame.GoBack();
    }
    const bool backReady = m_currentPageId == homePageId
        && m_contentFrame.CanGoForward();

    if (m_contentFrame.CanGoForward())
    {
        m_contentFrame.GoForward();
    }
    const bool forwardReady = m_currentPageId == aboutPageId;

    navigateTo(homePageId);
    return homeReady && aboutReady && backReady && forwardReady;
}

bool MainWindow::runPhase3LocalizationChecks()
{
    constexpr std::wstring_view actionContext = L"ActionRegistry";
    constexpr std::wstring_view aboutSource = L"About";
    constexpr std::wstring_view informationSource =
        L"Show application information";

    const std::array<std::wstring_view, 4> englishTags{
        L"en-AU",
        L"en-CA",
        L"en-GB",
        L"en-US"
    };
    for (const auto languageTag : englishTags)
    {
        const WinUILocalizer english(languageTag);
        if (!english.hasString(actionContext, aboutSource)
            || english.getString(actionContext, aboutSource) != L"About"
            || english.getString(actionContext, informationSource)
                != L"Show application information")
        {
            return false;
        }
    }

    const WinUILocalizer korean(L"ko-KR");
    return korean.hasString(actionContext, aboutSource)
        && korean.getString(actionContext, aboutSource) == L"정보"
        && korean.getString(actionContext, informationSource)
            == L"애플리케이션 정보 표시"
        && !korean.hasString(L"MissingContext", L"Missing resource")
        && WinUILocalizer::makeResourceId(actionContext, aboutSource)
            != WinUILocalizer::makeResourceId(actionContext, L"about");
}

bool MainWindow::runPhase3DialogChecks()
{
    if (!ensureHomePage() || !ClassMngrWinUIDialogs::runDialogContractChecks()
        || !m_homeViewModel || !m_progressRing || !m_cancelButton
        || !m_validationSummaryText || !m_unsavedChangesButton)
    {
        return false;
    }

    classmngr::engine::ValidationResult validation;
    validation.add(classmngr::engine::ValidationIssue{
        "required",
        "name",
        classmngr::engine::ValidationSeverity::Error,
        0,
        0
        });
    presentValidationSummary(validation);
    const bool validationPresented = m_homeViewModel->HasValidationErrors()
        && std::wstring_view(
               m_validationSummaryText.Text().c_str(),
               m_validationSummaryText.Text().size()
               ).find(L"code=required") != std::wstring_view::npos;

    m_homeViewModel->ClearValidation();
    updateHomePresentation();
    return validationPresented
        && !m_progressRing.IsActive()
        && !m_cancelButton.IsEnabled()
        && m_unsavedChangesButton.IsTabStop()
        && m_validationSummaryText.Text() == L"No validation issues.";
}

Windows::Foundation::IAsyncOperation<bool>
MainWindow::runPhase3SemanticChecks()
{
    auto lifetime = get_strong();
    const bool navigationReady = runPhase3NavigationChecks();
    const bool inputReady = runPhase1InputChecks();

    bool focusReady = false;
    if (inputReady)
    {
        navigateTo(aboutPageId);
        const bool aboutPageReady = m_currentPageId == aboutPageId;
        navigateTo(homePageId);
        if (aboutPageReady)
        {
            // Navigation creates the Home controls synchronously, but they do
            // not become focusable until the next dispatcher turn applies the
            // pending layout. Keep the focus assertion meaningful by waiting
            // for that UI turn rather than treating an unattached control as
            // a focus failure.
            co_await ResumeOnDispatcherQueue{
                DispatcherQueue(),
                Microsoft::UI::Dispatching::DispatcherQueuePriority::Low
                };
        }
        if (aboutPageReady && ensureHomePage() && m_nameTextBox.XamlRoot())
        {
            const bool focusRequested = m_nameTextBox.Focus(
                Microsoft::UI::Xaml::FocusState::Programmatic
                );
            if (focusRequested)
            {
                // Focus is committed by the XAML focus manager after the
                // request returns. Observe the manager on a later UI turn.
                co_await ResumeOnDispatcherQueue{
                    DispatcherQueue(),
                    Microsoft::UI::Dispatching::DispatcherQueuePriority::Low
                    };
                const auto focusedElement =
                    Microsoft::UI::Xaml::Input::FocusManager::GetFocusedElement(
                        m_nameTextBox.XamlRoot()
                        );
                focusReady = focusedElement == m_nameTextBox;
            }
        }
    }

    const bool resourcesReady = runPhase3LocalizationChecks();
    const bool dialogsReady = runPhase3DialogChecks();
    const bool threadingReady = ClassMngrWinUIThreading::runThreadingContractChecks();
    const bool phase4Ready = runPhase4SemanticChecks();
    if (!navigationReady || !inputReady || !focusReady || !resourcesReady
        || !dialogsReady || !threadingReady || !phase4Ready)
    {
        co_return false;
    }

    co_return co_await runPhase3ViewModelChecks();
}

bool MainWindow::runPhase4SemanticChecks()
{
    if (!ensureHomePage() || !m_scheduleSlotTextBox || !m_scheduleStatusText
        || !m_rosterSourceList || !m_rosterTransferredList || !m_rosterStatusText
        || !m_speakingPasteTextBox || !m_speakingStatusText
        || m_speakingScoreCells.size() != 9)
    {
        return false;
    }

    const auto eventArguments = Microsoft::UI::Xaml::RoutedEventArgs();
    m_scheduleSlotTextBox.Text(L"09:45–10:30");
    ScheduleApplyButton_Click(nullptr, eventArguments);
    const bool scheduleReady = std::wstring_view(
        m_scheduleStatusText.Text().c_str(),
        m_scheduleStatusText.Text().size()
        ).find(L"not persisted") != std::wstring_view::npos;

    const auto sourceCount = m_rosterSourceList.Items().Size();
    const auto transferredCount = m_rosterTransferredList.Items().Size();
    m_rosterSourceList.SelectedIndex(0);
    RosterTransferButton_Click(nullptr, eventArguments);
    const bool rosterReady = sourceCount > 0
        && m_rosterSourceList.Items().Size() + 1 == sourceCount
        && m_rosterTransferredList.Items().Size() == transferredCount + 1
        && m_rosterTransferredList.SelectedIndex() >= 0;

    m_speakingPasteTextBox.Text(L"8\t7\t9\n9\t8\t8");
    SpeakingPasteButton_Click(nullptr, eventArguments);
    const bool pasteReady = m_speakingScoreCells[0].Text() == L"8"
        && m_speakingScoreCells[1].Text() == L"7"
        && m_speakingScoreCells[5].Text() == L"8"
        && std::wstring_view(
               m_speakingStatusText.Text().c_str(),
               m_speakingStatusText.Text().size()
               ).find(L"Applied 6") != std::wstring_view::npos;

    SpeakingAnalyticsButton_Click(nullptr, eventArguments);
    const bool analyticsReady = std::wstring_view(
        m_speakingStatusText.Text().c_str(),
        m_speakingStatusText.Text().size()
        ).find(L"Analytics navigation requested") != std::wstring_view::npos;

    return scheduleReady && rosterReady && pasteReady && analyticsReady
        && m_dirtyState.isDirty();
}

Windows::Foundation::IAsyncOperation<bool>
MainWindow::runPhase3ViewModelChecks()
{
    auto lifetime = get_strong();
    auto viewModel = winrt::make_self<ObservableViewModel>();
    std::vector<std::wstring> changedProperties;
    const auto observable = viewModel.as<
        Microsoft::UI::Xaml::Data::INotifyPropertyChanged>();
    const auto propertyToken = observable.PropertyChanged(
        [&changedProperties](
            Windows::Foundation::IInspectable const&,
            Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& arguments
            ) {
            changedProperties.emplace_back(
                arguments.PropertyName().c_str(),
                arguments.PropertyName().size()
                );
        }
        );

    viewModel->PresentError(classmngr::engine::Error{
        classmngr::engine::ErrorCode::InvalidArgument,
        "A test error.",
        42
        });
    classmngr::engine::ValidationResult validation;
    validation.add(classmngr::engine::ValidationIssue{
        "required",
        "name",
        classmngr::engine::ValidationSeverity::Error,
        3,
        1
        });
    viewModel->PresentValidation(validation);

    const auto contains = [](winrt::hstring const& value, std::wstring_view text) {
        return std::wstring_view(value.c_str(), value.size()).find(text)
            != std::wstring_view::npos;
    };
    const bool presentationReady = viewModel->HasError()
        && contains(viewModel->ErrorMessage(), L"invalid-argument")
        && contains(viewModel->ErrorMessage(), L"native-code=42")
        && viewModel->HasValidationErrors()
        && contains(viewModel->ValidationSummary(), L"code=required")
        && contains(viewModel->ValidationSummary(), L"field=name")
        && changedProperties.size() >= 4;
    observable.PropertyChanged(propertyToken);
    if (!presentationReady)
    {
        co_return false;
    }

    struct CommandCheckState
    {
        winrt::handle completion{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
        winrt::handle workStarted{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
        std::atomic_bool workInvoked{};
        std::atomic_bool completionReady{};
        std::atomic_uint32_t stateChanges{};
    };
    auto commandState = std::make_shared<CommandCheckState>();
    if (!commandState->completion || !commandState->workStarted)
    {
        co_return false;
    }

    AsyncCommand::AsyncWork work = [commandState](
        classmngr::engine::CancellationToken const& cancellation
        ) {
        commandState->workInvoked.store(true, std::memory_order_relaxed);
        SetEvent(commandState->workStarted.get());
        return phase3PresentationWork(cancellation);
    };
    auto command = winrt::make_self<AsyncCommand>(
        DispatcherQueue(),
        std::move(work)
        );
    const auto commandInterface = command.as<Microsoft::UI::Xaml::Input::ICommand>();
    const auto weakCommand = command->get_weak();
    const auto commandToken = commandInterface.CanExecuteChanged(
        [commandState, weakCommand](
            Windows::Foundation::IInspectable const&,
            Windows::Foundation::IInspectable const&
            ) {
            commandState->stateChanges.fetch_add(1, std::memory_order_relaxed);
            if (auto currentCommand = weakCommand.get();
                currentCommand && !currentCommand->IsRunning())
            {
                commandState->completionReady.store(
                    currentCommand->CanExecute(nullptr)
                        && commandState->stateChanges.load(
                            std::memory_order_relaxed
                            ) >= 2,
                    std::memory_order_relaxed
                    );
                SetEvent(commandState->completion.get());
            }
        }
        );

    const bool initiallyEnabled = commandInterface.CanExecute(nullptr);
    commandInterface.Execute(nullptr);
    co_await winrt::resume_on_signal(commandState->workStarted.get());
    const bool runningAfterExecute = command->IsRunning()
        && !commandInterface.CanExecute(nullptr)
        && commandState->workInvoked.load(std::memory_order_relaxed);
    command->Cancel();
    const bool cancellationRequested = command->IsCancellationRequested();
    if (!initiallyEnabled || !runningAfterExecute || !cancellationRequested)
    {
        commandInterface.CanExecuteChanged(commandToken);
        co_return false;
    }

    co_await winrt::resume_on_signal(commandState->completion.get());
    static_cast<void>(commandToken);
    co_return commandState->completionReady.load(std::memory_order_relaxed);
}

void MainWindow::ContinueButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);

    if (!m_nameTextBox || !m_homeViewModel || !m_homeCommand)
    {
        return;
    }

    if (m_nameTextBox.Text().empty())
    {
        classmngr::engine::ValidationResult validation;
        validation.add(classmngr::engine::ValidationIssue{
            "required",
            "name",
            classmngr::engine::ValidationSeverity::Error,
            0,
            0
            });
        presentValidationSummary(validation);
        m_statusText.Text(L"Enter a name.");
        return;
    }

    if (m_homeCommand->CanExecute(nullptr))
    {
        m_homeViewModel->ClearValidation();
        m_homeCommand->Execute(nullptr);
        updateHomePresentation();
    }
}

void MainWindow::ShellInfoButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    showOwnedDialog();
}

void MainWindow::CancelButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_homeCommand)
    {
        m_homeCommand->Cancel();
        updateHomePresentation();
    }
}

void MainWindow::UnsavedChangesButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    showUnsavedChangesConfirmation();
}

void MainWindow::ScheduleApplyButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_scheduleSlotTextBox || !m_scheduleStatusText)
    {
        return;
    }

    m_dirtyState.markDirty();
    m_scheduleStatusText.Text(
        L"Time slot label updated in the prototype (not persisted)."
        );
}

void MainWindow::RosterSource_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    const auto list = sender.try_as<Microsoft::UI::Xaml::Controls::ListViewBase>();
    if (!m_rosterStatusText || !list)
    {
        return;
    }

    const auto selected = list.SelectedItem().try_as<
        Microsoft::UI::Xaml::Controls::TextBox>();
    if (selected)
    {
        m_rosterStatusText.Text(
            winrt::hstring(L"Selected " + std::wstring(
                selected.Text().c_str(), selected.Text().size()
                ) + L" for transfer.")
            );
    }
}

void MainWindow::RosterTransferButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_rosterSourceList || !m_rosterTransferredList
        || !m_rosterStatusText)
    {
        return;
    }

    const int32_t selectedIndex = m_rosterSourceList.SelectedIndex();
    const auto selected = m_rosterSourceList.SelectedItem().try_as<
        Microsoft::UI::Xaml::Controls::TextBox>();
    if (selectedIndex < 0 || !selected)
    {
        m_rosterStatusText.Text(L"Select a student before transferring.");
        return;
    }

    auto transferred = Microsoft::UI::Xaml::Controls::TextBox();
    transferred.Text(selected.Text());
    transferred.Header(winrt::box_value(winrt::hstring(L"Student name")));
    transferred.IsTabStop(true);
    setAutomationName(transferred, L"Transferred student name");
    transferred.TextChanging({this, &MainWindow::NameTextBox_TextChanged});
    m_rosterSourceList.Items().RemoveAt(
        static_cast<uint32_t>(selectedIndex)
        );
    m_rosterTransferredList.Items().Append(transferred);
    m_rosterTransferredList.SelectedItem(transferred);
    m_dirtyState.markDirty();
    m_rosterStatusText.Text(L"Student transferred in the prototype.");
}

void MainWindow::SpeakingPasteButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_speakingPasteTextBox || !m_speakingStatusText)
    {
        return;
    }

    const auto rows = parsePastedRange(std::wstring_view(
        m_speakingPasteTextBox.Text().c_str(),
        m_speakingPasteTextBox.Text().size()
        ));
    constexpr size_t scoreColumns = 3;
    size_t applied = 0;
    for (size_t row = 0; row < rows.size() && row < 3; ++row)
    {
        for (size_t column = 0;
             column < rows[row].size() && column < scoreColumns;
             ++column)
        {
            const size_t cellIndex = row * scoreColumns + column;
            if (cellIndex < m_speakingScoreCells.size())
            {
                m_speakingScoreCells[cellIndex].Text(
                    winrt::hstring(rows[row][column])
                    );
                ++applied;
            }
        }
    }

    if (applied == 0)
    {
        m_speakingStatusText.Text(L"Paste a tab/newline range to apply scores.");
        return;
    }

    m_dirtyState.markDirty();
    m_speakingStatusText.Text(winrt::hstring(
        L"Applied " + std::to_wstring(applied)
            + L" pasted score cells in the prototype."
        ));
}

void MainWindow::SpeakingAnalyticsButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_speakingStatusText)
    {
        m_speakingStatusText.Text(
            L"Analytics navigation requested for the selected roster."
            );
    }
}

void MainWindow::NavigationView_SelectionChanged(
    Microsoft::UI::Xaml::Controls::NavigationView const& sender,
    Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    if (m_selectionChanging)
    {
        return;
    }

    const auto selectedItem = arguments.SelectedItem().try_as<
        Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    if (!selectedItem)
    {
        return;
    }

    navigateTo(boxedString(selectedItem.Tag()));
}

void MainWindow::NavigationView_BackRequested(
    Microsoft::UI::Xaml::Controls::NavigationView const& sender,
    Microsoft::UI::Xaml::Controls::NavigationViewBackRequestedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_contentFrame.CanGoBack())
    {
        m_contentFrame.GoBack();
    }
    updateNavigationState();
}

void MainWindow::ContentFrame_Navigated(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& arguments
    )
{
    static_cast<void>(sender);

    const auto page = arguments.Content().try_as<
        Microsoft::UI::Xaml::Controls::Page>();
    if (!page)
    {
        return;
    }

    std::wstring pageId = boxedString(arguments.Parameter());
    if (!isKnownPageId(pageId))
    {
        pageId = std::wstring(homePageId);
    }
    populatePage(page, pageId);
    m_currentPageId = pageId;

    m_selectionChanging = true;
    m_navigationView.SelectedItem(
        pageId == homePageId
            ? m_homeNavigationItem
            : m_aboutNavigationItem
        );
    m_selectionChanging = false;
    updateNavigationState();
    if (!m_restoringState)
    {
        saveShellState();
    }
}

void MainWindow::Window_Activated(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::WindowActivatedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_windowBoundsRestored)
    {
        restoreWindowBounds();
        m_windowBoundsRestored = true;
    }
}

void MainWindow::Window_Closed(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::WindowEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    saveShellState();
}

void MainWindow::navigateTo(std::wstring_view pageId)
{
    if (!isKnownPageId(pageId))
    {
        return;
    }

    if (m_currentPageId == pageId && m_contentFrame.Content())
    {
        updateNavigationState();
        return;
    }

    static_cast<void>(m_contentFrame.Navigate(
        winrt::xaml_typename<Microsoft::UI::Xaml::Controls::Page>(),
        winrt::box_value(winrt::hstring(pageId))
        ));
}

void MainWindow::populatePage(
    Microsoft::UI::Xaml::Controls::Page const& page,
    std::wstring_view pageId
    )
{
    if (page.Content())
    {
        if (pageId == homePageId && !m_engineVersionText)
        {
            populateHomePage(page);
        }
        return;
    }

    if (pageId == homePageId)
    {
        populateHomePage(page);
    }
    else
    {
        populateAboutPage(page);
    }
}

void MainWindow::populateHomePage(
    Microsoft::UI::Xaml::Controls::Page const& page
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    auto root = StackPanel();
    root.Padding(Thickness{32.0, 32.0, 32.0, 32.0});
    root.Spacing(16.0);
    root.MaxWidth(720.0);
    root.HorizontalAlignment(HorizontalAlignment::Center);
    root.VerticalAlignment(VerticalAlignment::Top);

    auto title = TextBlock();
    title.Text(L"ClassMngr Windows");
    title.FontSize(32.0);
    setAutomationName(title, L"ClassMngr Windows");

    m_engineVersionText = TextBlock();
    m_engineVersionText.FontSize(16.0);
    m_engineVersionText.Text(
        winrt::to_hstring(
            std::string("Engine version: ") + m_engineVersion.toString()
            )
        );
    setAutomationName(m_engineVersionText, L"Engine version");

    auto description = TextBlock();
    description.Text(L"The Windows shell is ready for feature pages.");
    description.TextWrapping(TextWrapping::Wrap);

    m_nameTextBox = TextBox();
    m_nameTextBox.Header(winrt::box_value(winrt::hstring(L"이름")));
    m_nameTextBox.PlaceholderText(L"한국어 입력");
    auto inputScope = Input::InputScope();
    inputScope.Names().Append(
        Input::InputScopeName(Input::InputScopeNameValue::Text)
        );
    m_nameTextBox.InputScope(inputScope);
    m_nameTextBox.TabIndex(0);
    m_nameTextBox.IsTabStop(true);
    m_nameTextBox.TextChanging({this, &MainWindow::NameTextBox_TextChanged});
    setAutomationName(m_nameTextBox, L"Name input");

    m_continueButton = Button();
    m_continueButton.Content(winrt::box_value(winrt::hstring(L"Continue")));
    m_continueButton.TabIndex(1);
    m_continueButton.IsTabStop(true);
    m_continueButton.HorizontalAlignment(HorizontalAlignment::Left);
    m_continueButton.Click({this, &MainWindow::ContinueButton_Click});
    setAutomationName(m_continueButton, L"Continue");

    m_progressRing = ProgressRing();
    m_progressRing.IsActive(false);
    m_progressRing.Visibility(Visibility::Collapsed);
    m_progressRing.Width(24.0);
    m_progressRing.Height(24.0);
    m_progressRing.HorizontalAlignment(HorizontalAlignment::Left);
    setAutomationName(m_progressRing, L"Operation progress");

    m_cancelButton = Button();
    m_cancelButton.Content(winrt::box_value(winrt::hstring(L"Cancel operation")));
    m_cancelButton.TabIndex(2);
    m_cancelButton.IsTabStop(true);
    m_cancelButton.IsEnabled(false);
    m_cancelButton.HorizontalAlignment(HorizontalAlignment::Left);
    m_cancelButton.Click({this, &MainWindow::CancelButton_Click});
    setAutomationName(m_cancelButton, L"Cancel current operation");

    m_statusText = TextBlock();
    m_statusText.Text(L"Ready");
    m_statusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_statusText, L"Status");

    m_validationSummaryText = TextBlock();
    m_validationSummaryText.Text(L"No validation issues.");
    m_validationSummaryText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_validationSummaryText, L"Validation summary");

    m_unsavedChangesButton = Button();
    m_unsavedChangesButton.Content(
        winrt::box_value(winrt::hstring(L"Review unsaved changes"))
        );
    m_unsavedChangesButton.TabIndex(3);
    m_unsavedChangesButton.IsTabStop(true);
    m_unsavedChangesButton.HorizontalAlignment(HorizontalAlignment::Left);
    m_unsavedChangesButton.Click({this, &MainWindow::UnsavedChangesButton_Click});
    setAutomationName(m_unsavedChangesButton, L"Review unsaved changes");

    auto scheduleCard = ClassMngrWinUISharedUX::buildCard({
        L"Schedule/time-slot editing prototype",
        L"Edit a representative time-slot label in memory. This prototype does not persist data or apply engine rules.",
        L"Schedule editor"
        });
    m_scheduleSlotTextBox = TextBox();
    m_scheduleSlotTextBox.Header(
        winrt::box_value(winrt::hstring(L"Time-slot label"))
        );
    m_scheduleSlotTextBox.Text(L"09:00–09:45");
    m_scheduleSlotTextBox.PlaceholderText(L"e.g. 09:00–09:45");
    m_scheduleSlotTextBox.IsTabStop(true);
    m_scheduleSlotTextBox.TabIndex(4);
    m_scheduleSlotTextBox.TextChanging({this, &MainWindow::NameTextBox_TextChanged});
    setAutomationName(m_scheduleSlotTextBox, L"Schedule time-slot label editor");

    auto scheduleApply = Button();
    scheduleApply.Content(winrt::box_value(winrt::hstring(L"Apply time-slot label")));
    scheduleApply.IsTabStop(true);
    scheduleApply.TabIndex(5);
    scheduleApply.HorizontalAlignment(HorizontalAlignment::Left);
    scheduleApply.Click({this, &MainWindow::ScheduleApplyButton_Click});
    setAutomationName(scheduleApply, L"Schedule apply time-slot label");

    m_scheduleStatusText = TextBlock();
    m_scheduleStatusText.Text(L"Ready to edit a time slot.");
    m_scheduleStatusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_scheduleStatusText, L"Schedule editor status");
    scheduleCard.content.Children().Append(m_scheduleSlotTextBox);
    scheduleCard.content.Children().Append(scheduleApply);
    scheduleCard.content.Children().Append(m_scheduleStatusText);

    auto rosterCard = ClassMngrWinUISharedUX::buildCard({
        L"Roster selection, transfer, and keyboard editing prototype",
        L"Select a student, edit the name with the keyboard, and simulate a transfer between lists.",
        L"Roster editor"
        });
    auto rosterLists = StackPanel();
    rosterLists.Orientation(Orientation::Horizontal);
    rosterLists.Spacing(8.0);

    m_rosterSourceList = ListView();
    m_rosterSourceList.Header(
        winrt::box_value(winrt::hstring(L"Available students"))
        );
    m_rosterSourceList.SelectionMode(ListViewSelectionMode::Single);
    m_rosterSourceList.IsTabStop(true);
    m_rosterSourceList.Width(220.0);
    m_rosterSourceList.Height(150.0);
    setAutomationName(m_rosterSourceList, L"Roster available students");
    m_rosterSourceList.SelectionChanged({this, &MainWindow::RosterSource_SelectionChanged});
    for (auto const& name : {L"김민서", L"Alex Kim", L"박서준"})
    {
        auto student = TextBox();
        student.Header(winrt::box_value(winrt::hstring(L"Student name")));
        student.Text(name);
        student.IsTabStop(true);
        student.TextChanging({this, &MainWindow::NameTextBox_TextChanged});
        setAutomationName(student, L"Roster editable student name");
        m_rosterSourceList.Items().Append(student);
    }

    m_rosterTransferredList = ListView();
    m_rosterTransferredList.Header(
        winrt::box_value(winrt::hstring(L"Transferred students"))
        );
    m_rosterTransferredList.SelectionMode(ListViewSelectionMode::Single);
    m_rosterTransferredList.IsTabStop(true);
    m_rosterTransferredList.Width(220.0);
    m_rosterTransferredList.Height(150.0);
    setAutomationName(m_rosterTransferredList, L"Roster transferred students");

    rosterLists.Children().Append(m_rosterSourceList);
    rosterLists.Children().Append(m_rosterTransferredList);
    auto rosterTransfer = Button();
    rosterTransfer.Content(winrt::box_value(winrt::hstring(L"Transfer selected →")));
    rosterTransfer.IsTabStop(true);
    rosterTransfer.TabIndex(6);
    rosterTransfer.HorizontalAlignment(HorizontalAlignment::Left);
    rosterTransfer.Click({this, &MainWindow::RosterTransferButton_Click});
    setAutomationName(rosterTransfer, L"Roster transfer selected student");
    m_rosterStatusText = TextBlock();
    m_rosterStatusText.Text(L"Select a roster row to begin.");
    m_rosterStatusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_rosterStatusText, L"Roster selected student status");
    rosterCard.content.Children().Append(rosterLists);
    rosterCard.content.Children().Append(rosterTransfer);
    rosterCard.content.Children().Append(m_rosterStatusText);

    auto speakingCard = ClassMngrWinUISharedUX::buildCard({
        L"Speaking-evaluation scores and analytics prototype",
        L"Edit score cells, apply a tab/newline range, and request analytics navigation using standard controls.",
        L"Speaking evaluation editor"
        });
    m_speakingScoreCells.clear();
    auto scoreGrid = Grid();
    for (size_t column = 0; column < 4; ++column)
    {
        scoreGrid.ColumnDefinitions().Append(ColumnDefinition());
    }
    for (size_t row = 0; row < 4; ++row)
    {
        scoreGrid.RowDefinitions().Append(RowDefinition());
    }
    setAutomationName(scoreGrid, L"Speaking evaluation score grid");

    auto addScoreHeader = [&scoreGrid](wchar_t const* text, uint32_t row, uint32_t column) {
        auto header = TextBlock();
        header.Text(text);
        header.Margin(Thickness{4.0, 2.0, 4.0, 2.0});
        Grid::SetRow(header, row);
        Grid::SetColumn(header, column);
        scoreGrid.Children().Append(header);
    };
    addScoreHeader(L"Student", 0, 0);
    addScoreHeader(L"Pronunciation", 0, 1);
    addScoreHeader(L"Fluency", 0, 2);
    addScoreHeader(L"Interaction", 0, 3);
    const std::array<wchar_t const*, 3> studentNames{
        L"김민서", L"Alex Kim", L"박서준"
    };
    const std::array<std::array<wchar_t const*, 3>, 3> initialScores{{
        {{L"8", L"7", L"9"}},
        {{L"9", L"8", L"8"}},
        {{L"7", L"8", L"7"}}
    }};
    for (uint32_t row = 0; row < 3; ++row)
    {
        addScoreHeader(studentNames[row], row + 1, 0);
        for (uint32_t column = 0; column < 3; ++column)
        {
            auto score = TextBox();
            score.Text(initialScores[row][column]);
            score.Width(76.0);
            score.Margin(Thickness{4.0, 2.0, 4.0, 2.0});
            score.IsTabStop(true);
            score.TabIndex(7 + row * 3 + column);
            score.TextChanging({this, &MainWindow::NameTextBox_TextChanged});
            const std::wstring scoreAutomationName =
                L"Speaking score " + std::to_wstring(row + 1) + L" "
                + std::to_wstring(column + 1);
            setAutomationName(score, scoreAutomationName);
            Grid::SetRow(score, row + 1);
            Grid::SetColumn(score, column + 1);
            scoreGrid.Children().Append(score);
            m_speakingScoreCells.emplace_back(score);
        }
    }

    m_speakingPasteTextBox = TextBox();
    m_speakingPasteTextBox.Header(
        winrt::box_value(winrt::hstring(L"Paste scores (tab/newline range)"))
        );
    m_speakingPasteTextBox.PlaceholderText(L"8\t7\t9\n9\t8\t8");
    m_speakingPasteTextBox.AcceptsReturn(true);
    m_speakingPasteTextBox.Height(72.0);
    m_speakingPasteTextBox.IsTabStop(true);
    m_speakingPasteTextBox.TabIndex(20);
    setAutomationName(m_speakingPasteTextBox, L"Speaking pasted score range");
    auto speakingPaste = Button();
    speakingPaste.Content(winrt::box_value(winrt::hstring(L"Apply pasted range")));
    speakingPaste.IsTabStop(true);
    speakingPaste.TabIndex(21);
    speakingPaste.HorizontalAlignment(HorizontalAlignment::Left);
    speakingPaste.Click({this, &MainWindow::SpeakingPasteButton_Click});
    setAutomationName(speakingPaste, L"Apply speaking pasted score range");
    auto speakingAnalytics = Button();
    speakingAnalytics.Content(winrt::box_value(winrt::hstring(L"Open analytics")));
    speakingAnalytics.IsTabStop(true);
    speakingAnalytics.TabIndex(22);
    speakingAnalytics.HorizontalAlignment(HorizontalAlignment::Left);
    speakingAnalytics.Click({this, &MainWindow::SpeakingAnalyticsButton_Click});
    setAutomationName(speakingAnalytics, L"Open speaking analytics");
    m_speakingStatusText = TextBlock();
    m_speakingStatusText.Text(L"Scores ready for editing.");
    m_speakingStatusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_speakingStatusText, L"Speaking analytics navigation status");
    speakingCard.content.Children().Append(scoreGrid);
    speakingCard.content.Children().Append(m_speakingPasteTextBox);
    speakingCard.content.Children().Append(speakingPaste);
    speakingCard.content.Children().Append(speakingAnalytics);
    speakingCard.content.Children().Append(m_speakingStatusText);

    m_homeViewModel = winrt::make_self<ObservableViewModel>();
    const auto command = winrt::make_self<AsyncCommand>(
        DispatcherQueue(),
        phase3PresentationWork
        );
    const auto commandInterface = command.as<Microsoft::UI::Xaml::Input::ICommand>();
    auto weak = get_weak();
    m_homeCommandStateToken = commandInterface.CanExecuteChanged(
        [weak](
            Windows::Foundation::IInspectable const&,
            Windows::Foundation::IInspectable const&
            ) {
            if (auto self = weak.get())
            {
                self->updateHomePresentation();
            }
        }
        );
    m_homeCommand = command;

    root.Children().Append(title);
    root.Children().Append(m_engineVersionText);
    root.Children().Append(description);
    root.Children().Append(m_nameTextBox);
    root.Children().Append(m_continueButton);
    root.Children().Append(m_progressRing);
    root.Children().Append(m_cancelButton);
    root.Children().Append(m_statusText);
    root.Children().Append(m_validationSummaryText);
    root.Children().Append(m_unsavedChangesButton);
    root.Children().Append(scheduleCard.root);
    root.Children().Append(rosterCard.root);
    root.Children().Append(speakingCard.root);
    page.Content(root);
}

void MainWindow::NameTextBox_TextChanged(
    Microsoft::UI::Xaml::Controls::TextBox const& sender,
    Microsoft::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    m_dirtyState.markDirty();
}

void MainWindow::populateAboutPage(
    Microsoft::UI::Xaml::Controls::Page const& page
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    auto root = StackPanel();
    root.Padding(Thickness{32.0, 32.0, 32.0, 32.0});
    root.Spacing(16.0);
    root.MaxWidth(720.0);
    root.HorizontalAlignment(HorizontalAlignment::Center);
    root.VerticalAlignment(VerticalAlignment::Top);

    auto title = TextBlock();
    title.Text(L"About ClassMngr");
    title.FontSize(28.0);
    setAutomationName(title, L"About ClassMngr");

    auto description = TextBlock();
    description.Text(
        L"This page is a lazy shell placeholder for the first feature slice."
        );
    description.TextWrapping(TextWrapping::Wrap);

    auto navigationContract = TextBlock();
    navigationContract.Text(
        L"Navigation history and shell state are restored by the main window."
        );
    navigationContract.TextWrapping(TextWrapping::Wrap);

    root.Children().Append(title);
    root.Children().Append(description);
    root.Children().Append(navigationContract);
    page.Content(root);
}

void MainWindow::restoreShellState()
{
    const auto state = loadShellState();
    m_restoringState = true;

    bool restored = false;
    if (!state.navigationState.empty())
    {
        try
        {
            m_contentFrame.SetNavigationState(
                winrt::hstring(state.navigationState)
                );
            restored = !m_currentPageId.empty();
        }
        catch (...)
        {
            restored = false;
        }
    }

    if (!restored)
    {
        navigateTo(
            isKnownPageId(state.selectedPage)
                ? std::wstring_view(state.selectedPage)
                : homePageId
            );
    }
    m_restoringState = false;
    updateNavigationState();
}

void MainWindow::restoreWindowBounds() noexcept
{
    const auto state = loadShellState();
    if (!state.hasWindowBounds || !isUsableWindowBounds(state.windowBounds))
    {
        return;
    }

    HWND const handle = windowHandle(this);
    if (!handle)
    {
        return;
    }

    const LONG width = state.windowBounds.right - state.windowBounds.left;
    const LONG height = state.windowBounds.bottom - state.windowBounds.top;
    SetWindowPos(
        handle,
        nullptr,
        state.windowBounds.left,
        state.windowBounds.top,
        width,
        height,
        SWP_NOZORDER | SWP_NOACTIVATE
        );
}

void MainWindow::saveShellState() noexcept
{
    HKEY const key = openShellStateForWrite();
    if (!key)
    {
        return;
    }

    writeRegistryString(
        key,
        L"SelectedPage",
        selectedPageId()
        );
    try
    {
        const auto navigationState = m_contentFrame.GetNavigationState();
        writeRegistryString(
            key,
            L"NavigationState",
            asWString(navigationState)
            );
    }
    catch (...)
    {
    }

    HWND const handle = windowHandle(this);
    RECT bounds{};
    if (handle && GetWindowRect(handle, &bounds))
    {
        writeRegistryDword(key, L"WindowLeft", static_cast<DWORD>(bounds.left));
        writeRegistryDword(key, L"WindowTop", static_cast<DWORD>(bounds.top));
        writeRegistryDword(key, L"WindowRight", static_cast<DWORD>(bounds.right));
        writeRegistryDword(key, L"WindowBottom", static_cast<DWORD>(bounds.bottom));
    }

    RegCloseKey(key);
}

void MainWindow::updateNavigationState()
{
    if (m_navigationView && m_contentFrame)
    {
        m_navigationView.IsBackEnabled(m_contentFrame.CanGoBack());
    }
}

void MainWindow::showOwnedDialog()
{
    showDialog(
        L"ClassMngr shell",
        L"This dialog is owned by MainWindow and uses the active XamlRoot.",
        {},
        {},
        L"Close",
        {}
        );
}

void MainWindow::showUnsavedChangesConfirmation()
{
    if (!m_dirtyState.isDirty())
    {
        if (m_statusText)
        {
            m_statusText.Text(L"No unsaved changes.");
        }
        return;
    }

    auto weak = get_weak();
    showDialog(
        L"Unsaved changes",
        L"Choose Save to let the feature save changes, Discard to discard them, or Keep editing to cancel.",
        L"Save",
        L"Discard",
        L"Keep editing",
        [weak](ClassMngrWinUIDialogs::DialogOutcome outcome) {
            if (auto self = weak.get())
            {
                switch (ClassMngrWinUIDialogs::resolveUnsavedChanges(
                    self->m_dirtyState,
                    outcome
                    ))
                {
                case ClassMngrWinUIDialogs::UnsavedChangesDecision::Save:
                    self->m_statusText.Text(
                        L"Save requested; changes remain dirty until the feature completes it."
                        );
                    break;
                case ClassMngrWinUIDialogs::UnsavedChangesDecision::Discard:
                    self->m_dirtyState.markClean();
                    self->m_statusText.Text(L"Unsaved changes discarded.");
                    break;
                case ClassMngrWinUIDialogs::UnsavedChangesDecision::Stay:
                    self->m_statusText.Text(L"Continuing to edit unsaved changes.");
                    break;
                case ClassMngrWinUIDialogs::UnsavedChangesDecision::Proceed:
                    self->m_statusText.Text(L"No unsaved changes.");
                    break;
                }
            }
        }
        );
}

void MainWindow::showDialog(
    winrt::hstring const& title,
    winrt::hstring const& content,
    winrt::hstring const& primaryText,
    winrt::hstring const& secondaryText,
    winrt::hstring const& closeText,
    std::function<void(ClassMngrWinUIDialogs::DialogOutcome)> completion
    )
{
    const auto xamlRoot = RootGrid().XamlRoot();
    if (m_ownedDialog || !m_contentFrame || !xamlRoot)
    {
        return;
    }

    auto dialog = Microsoft::UI::Xaml::Controls::ContentDialog();
    dialog.XamlRoot(xamlRoot);
    dialog.Title(winrt::box_value(title));
    dialog.Content(winrt::box_value(content));
    dialog.PrimaryButtonText(primaryText);
    dialog.SecondaryButtonText(secondaryText);
    dialog.CloseButtonText(closeText);
    dialog.DefaultButton(
        primaryText.empty()
            ? Microsoft::UI::Xaml::Controls::ContentDialogButton::Close
            : Microsoft::UI::Xaml::Controls::ContentDialogButton::Primary
        );
    m_ownedDialog = dialog;
    completeOwnedDialog(dialog, std::move(completion));
}

winrt::fire_and_forget MainWindow::completeOwnedDialog(
    Microsoft::UI::Xaml::Controls::ContentDialog dialog,
    std::function<void(ClassMngrWinUIDialogs::DialogOutcome)> completion
    )
{
    auto lifetime = get_strong();
    ClassMngrWinUIDialogs::DialogOutcome outcome =
        ClassMngrWinUIDialogs::DialogOutcome::Cancel;
    try
    {
        const auto result = co_await dialog.ShowAsync();
        if (result == Microsoft::UI::Xaml::Controls::ContentDialogResult::Primary)
        {
            outcome = ClassMngrWinUIDialogs::DialogOutcome::Primary;
        }
        else if (result
            == Microsoft::UI::Xaml::Controls::ContentDialogResult::Secondary)
        {
            outcome = ClassMngrWinUIDialogs::DialogOutcome::Secondary;
        }
    }
    catch (...)
    {
        // Hiding during shell teardown is cancellation, not an error dialog.
    }

    if (m_ownedDialog == dialog)
    {
        m_ownedDialog = nullptr;
        if (completion)
        {
            completion(outcome);
        }
    }
}

void MainWindow::updateHomePresentation()
{
    if (!m_homeCommand || !m_progressRing || !m_cancelButton || !m_statusText)
    {
        return;
    }

    const bool running = m_homeCommand->IsRunning();
    m_progressRing.IsActive(running);
    m_progressRing.Visibility(
        running
            ? Microsoft::UI::Xaml::Visibility::Visible
            : Microsoft::UI::Xaml::Visibility::Collapsed
        );
    m_cancelButton.IsEnabled(running);
    if (running)
    {
        m_statusText.Text(
            m_homeCommand->IsCancellationRequested()
                ? L"Cancelling operation..."
                : L"Operation in progress..."
            );
    }
    else if (m_statusText.Text() == L"Operation in progress..."
        || m_statusText.Text() == L"Cancelling operation...")
    {
        m_statusText.Text(L"Operation complete.");
    }

    if (m_validationSummaryText && m_homeViewModel)
    {
        m_validationSummaryText.Text(
            m_homeViewModel->HasValidationErrors()
                ? m_homeViewModel->ValidationSummary()
                : L"No validation issues."
            );
    }
}

void MainWindow::presentValidationSummary(
    classmngr::engine::ValidationResult const& validation
    )
{
    if (!m_homeViewModel)
    {
        return;
    }

    m_homeViewModel->PresentValidation(validation);
    updateHomePresentation();
}

void MainWindow::closeShell() noexcept
{
    saveShellState();
    try
    {
        if (m_selectionChangedToken.value != 0)
        {
            m_navigationView.SelectionChanged(m_selectionChangedToken);
            m_selectionChangedToken = {};
        }
        if (m_backRequestedToken.value != 0)
        {
            m_navigationView.BackRequested(m_backRequestedToken);
            m_backRequestedToken = {};
        }
        if (m_navigatedToken.value != 0)
        {
            m_contentFrame.Navigated(m_navigatedToken);
            m_navigatedToken = {};
        }
        if (m_activatedToken.value != 0)
        {
            Activated(m_activatedToken);
            m_activatedToken = {};
        }
        if (m_closedToken.value != 0)
        {
            Closed(m_closedToken);
            m_closedToken = {};
        }
        if (m_ownedDialog)
        {
            m_ownedDialog.Hide();
            m_ownedDialog = nullptr;
        }
        if (m_homeCommand && m_homeCommandStateToken.value != 0)
        {
            m_homeCommand.as<Microsoft::UI::Xaml::Input::ICommand>()
                .CanExecuteChanged(m_homeCommandStateToken);
            m_homeCommandStateToken = {};
        }
        m_homeCommand = nullptr;
        m_homeViewModel = nullptr;
        m_contentFrame = nullptr;
        m_navigationView = nullptr;
    }
    catch (...)
    {
    }
}

std::wstring MainWindow::selectedPageId() const
{
    if (m_navigationView)
    {
        const auto selectedItem = m_navigationView.SelectedItem().try_as<
            Microsoft::UI::Xaml::Controls::NavigationViewItem>();
        if (selectedItem)
        {
            const std::wstring pageId = boxedString(selectedItem.Tag());
            if (isKnownPageId(pageId))
            {
                return pageId;
            }
        }
    }

    return isKnownPageId(m_currentPageId)
        ? m_currentPageId
        : std::wstring(homePageId);
}

bool MainWindow::ensureHomePage()
{
    if (m_currentPageId != homePageId)
    {
        navigateTo(homePageId);
    }
    return m_currentPageId == homePageId
        && static_cast<bool>(m_nameTextBox)
        && static_cast<bool>(m_continueButton)
        && static_cast<bool>(m_statusText);
}

} // namespace winrt::ClassMngrWinUI::implementation
