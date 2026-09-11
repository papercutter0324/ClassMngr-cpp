#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::preparePhase5CampusScenario(std::wstring_view scenario)
{
    static_cast<void>(preparePhase5CampusFixture(scenario));
    navigateTo(campusInformationPageId);
    refreshCampusInformationPage();
    updateFileCommandState();
}

bool MainWindow::preparePhase5CampusFixture(std::wstring_view scenario)
{
    const bool noDatabase = scenario == L"no-database";
    const bool empty = scenario == L"empty";
    const bool populated = scenario == L"populated";
    const bool error = scenario == L"error";
    m_phase5CampusScenario = error || (!noDatabase && !empty && !populated)
        ? L"error"
        : std::wstring(scenario);
    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();

    if (!noDatabase && m_phase5CampusScenario != L"error")
    {
        auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
        if (!opened || *opened == nullptr)
        {
            m_phase5CampusScenario = L"error";
        }
        else
        {
            if (populated)
            {
                classmngr::engine::CampusRecordService service(**opened);
                const auto makeCampus = [](wchar_t const* name,
                                           char const* imagePath) {
                    classmngr::engine::CampusRecord campus;
                    campus.name = winrt::to_string(winrt::hstring(name));
                    campus.buildingName = "Main building";
                    campus.address = "Bundang-gu, Seongnam-si";
                    campus.phoneNumber = "+82-31-1234-5678";
                    campus.officeNumber = "Office 101";
                    campus.transitSteps = "Exit 3, then walk north.";
                    campus.arrivalInfo = "Check in at the information desk.";
                    campus.imagePath = imagePath;
                    campus.officeWifi = "TeacherNet";
                    campus.officeWifiPassword = "password";
                    campus.printerName = "Printer-1";
                    campus.printerSteps = "Load paper, then print.";
                    campus.photocopierCode = "42";
                    campus.housingLocations = "Bundang, Suji";
                    return campus;
                };
                const auto first = service.create(makeCampus(
                    L"\xBD84\xB2F9 \xCEA0\xD37C\xC2A4",
                    ":/assets/campuses/bundang/bundang_map.png"
                    ));
                const auto second = service.create(makeCampus(
                    L"\xCCAD\xAD6C \xCEA0\xD37C\xC2A4",
                    ":/assets/campuses/bundang/cheonggu_map.png"
                    ));
                if (!first || !second)
                {
                    m_phase5CampusScenario = L"error";
                }
            }
            if (m_phase5CampusScenario != L"error")
            {
                m_openDatabase = std::move(*opened);
            }
        }
    }

    updateFileCommandState();
    return m_phase5CampusScenario != L"error" && static_cast<bool>(m_openDatabase);
}

void MainWindow::startPhase5FirstNavigationMeasurement(
    std::function<void(bool)> completion
    )
{
    if (m_phase5FirstNavigationCompleted || m_phase5FirstNavigationAwaitingHome
        || m_phase5FirstNavigationStarted)
    {
        return;
    }

    m_phase5FirstNavigationCompletion = std::move(completion);
    m_contentFrame.CacheSize(0);
    m_contentFrame.BackStack().Clear();
    m_contentFrame.ForwardStack().Clear();
    m_contentFrame.CacheSize(3);
    if (!ensureHomePage())
    {
        completePhase5FirstNavigationMeasurement("Home shell was not ready.");
        return;
    }

    m_phase5FirstNavigationAwaitingHome = true;
    m_phase5FirstNavigationRenderingToken =
        Microsoft::UI::Xaml::Media::CompositionTarget::Rendering(
            {this, &MainWindow::Phase5FirstNavigation_Rendering}
            );
}

void MainWindow::Phase5FirstNavigation_Rendering(
    Windows::Foundation::IInspectable const& sender,
    Windows::Foundation::IInspectable const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);

    if (m_phase5FirstNavigationCompleted)
    {
        return;
    }

    if (m_phase5FirstNavigationAwaitingHome)
    {
        if (m_currentPageId != homePageId || !m_nameTextBox || !m_continueButton
            || !m_statusText)
        {
            return;
        }

        m_phase5FirstNavigationAwaitingHome = false;
        if (!preparePhase5CampusFixture(L"populated"))
        {
            completePhase5FirstNavigationMeasurement(
                "The populated Campus fixture could not be prepared."
                );
            return;
        }

        m_phase5FirstNavigationStarted = true;
        m_phase5FirstNavigationStart = std::chrono::steady_clock::now();
        navigateTo(campusInformationPageId);
        return;
    }

    if (!m_phase5FirstNavigationStarted)
    {
        return;
    }

    const auto renderingTime = std::chrono::steady_clock::now();
    constexpr uint32_t expectedRecordCount = 2;
    const bool pageReady = m_currentPageId == campusInformationPageId;
    const bool stateReady = m_campusInformationState == L"populated";
    const bool selectorReady = m_campusSelector
        && m_campusSelector.Items().Size() == expectedRecordCount;
    const bool selectedDetailReady = m_campusDetailsPanel
        && m_campusDetailsPanel.Children().Size() > 0
        && m_campusSelector.SelectedIndex() >= 0
        && m_campusSelector.SelectedIndex() < static_cast<int32_t>(expectedRecordCount);
    const bool recordCountReady = m_campusRecords.size() == expectedRecordCount
        && m_campusResourceRecords.size() == expectedRecordCount;

    if (pageReady && stateReady && selectorReady && selectedDetailReady
        && recordCountReady)
    {
        m_phase5FirstNavigationReady = renderingTime;
        completePhase5FirstNavigationMeasurement({});
    }
}

void MainWindow::completePhase5FirstNavigationMeasurement(
    std::string_view failure
    )
{
    if (m_phase5FirstNavigationCompleted)
    {
        return;
    }

    m_phase5FirstNavigationCompleted = true;
    if (m_phase5FirstNavigationRenderingToken.value != 0)
    {
        Microsoft::UI::Xaml::Media::CompositionTarget::Rendering(
            m_phase5FirstNavigationRenderingToken
            );
        m_phase5FirstNavigationRenderingToken = {};
    }

    const bool written = writePhase5FirstNavigationResult(failure);
    const auto completion = std::move(m_phase5FirstNavigationCompletion);
    if (completion)
    {
        completion(written && failure.empty());
    }
}

bool MainWindow::writePhase5FirstNavigationResult(std::string_view failure) const
{
    constexpr uint32_t expectedRecordCount = 2;
    const bool pageReady = m_currentPageId == campusInformationPageId;
    const bool stateReady = m_campusInformationState == L"populated";
    const bool selectorReady = m_campusSelector
        && m_campusSelector.Items().Size() == expectedRecordCount;
    const bool selectedDetailReady = m_campusDetailsPanel
        && m_campusDetailsPanel.Children().Size() > 0
        && m_campusSelector.SelectedIndex() >= 0
        && m_campusSelector.SelectedIndex() < static_cast<int32_t>(expectedRecordCount);
    const bool recordCountReady = m_campusRecords.size() == expectedRecordCount
        && m_campusResourceRecords.size() == expectedRecordCount;
    const bool ready = failure.empty() && m_phase5FirstNavigationStarted
        && pageReady && stateReady && selectorReady && selectedDetailReady
        && recordCountReady;

    std::string output;
    output.reserve(1024);
    output += "{\n  \"format\": \"classmngr.phase5.first-navigation.v1\"";
    output += ",\n  \"targetPage\": ";
    appendJsonEscaped(output, asUtf8(campusInformationPageId));
    output += ",\n  \"fixtureId\": \"phase5-campus-populated-v1\"";
    output += ",\n  \"expectedRecordCount\": 2";
    output += ",\n  \"cacheState\": \"frame-cache-cleared; home-rendered; campus-not-visited\"";
    output += ",\n  \"navigationStartEvent\": \"navigateTo(campus_information)\"";
    output += ",\n  \"navigationReadyEvent\": \"CompositionTarget::Rendering\"";
    output += ",\n  \"firstNavigationReadyMs\": ";
    if (m_phase5FirstNavigationStarted)
    {
        const auto elapsed = std::chrono::duration<double, std::milli>(
            m_phase5FirstNavigationReady - m_phase5FirstNavigationStart
            ).count();
        output += std::to_string(elapsed);
    }
    else
    {
        output += "null";
    }
    output += ",\n  \"semanticReadiness\": {\n";
    output += "    \"pageId\": ";
    appendJsonEscaped(output, asUtf8(m_currentPageId));
    output += ",\n    \"pageReady\": ";
    output += pageReady ? "true" : "false";
    output += ",\n    \"populatedState\": ";
    output += stateReady ? "true" : "false";
    output += ",\n    \"populatedListExists\": ";
    output += selectorReady ? "true" : "false";
    output += ",\n    \"selectedDetailPanelExists\": ";
    output += selectedDetailReady ? "true" : "false";
    output += ",\n    \"expectedRecordCountPresent\": ";
    output += recordCountReady ? "true" : "false";
    output += "\n  },\n  \"deferredImageStatus\": \"excluded-asynchronous\"";
    output += ",\n  \"ready\": ";
    output += ready ? "true" : "false";
    output += ",\n  \"failure\": ";
    if (failure.empty())
    {
        output += "null";
    }
    else
    {
        appendJsonEscaped(output, failure);
    }
    output += "\n}\n";

    try
    {
        std::ofstream result(
            std::filesystem::current_path() / L"phase5-first-navigation.json",
            std::ios::binary | std::ios::trunc
            );
        result.write(output.data(), static_cast<std::streamsize>(output.size()));
        return static_cast<bool>(result);
    }
    catch (...)
    {
        return false;
    }
}

Windows::Foundation::IAsyncOperation<bool>
MainWindow::runPhase3ViewModelChecks()
{
    auto lifetime = get_strong();
    const auto dispatcher = DispatcherQueue();
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
    co_await ResumeOnDispatcherQueue{
        dispatcher,
        Microsoft::UI::Dispatching::DispatcherQueuePriority::Normal
        };
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

} // namespace winrt::ClassMngrWinUI::implementation
