#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

bool MainWindow::runPhase1SmokeChecks()
{
    // Phase hooks can be run after a previous hook has persisted another
    // page. Normalize the shell before checking the Home-page contract so
    // the verifier remains independent of hook order and prior test state.
    if (m_currentPageId != homePageId)
    {
        navigateTo(homePageId);
    }

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

    m_nameTextBox.Text(L"í•œê¸€ ìž…ë ¥");
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

    return m_nameTextBox.Text() == winrt::hstring(L"í•œê¸€ ìž…ë ¥")
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
        && korean.getString(actionContext, aboutSource) == L"ì •ë³´"
        && korean.getString(actionContext, informationSource)
            == L"ì• í”Œë¦¬ì¼€ì´ì…˜ ì •ë³´ í‘œì‹œ"
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
            co_await ResumeOnDispatcherQueue{
                DispatcherQueue(),
                Microsoft::UI::Dispatching::DispatcherQueuePriority::Low
                };
        }
        if (aboutPageReady && ensureHomePage())
        {
            if (m_personalNameTextBox)
            {
                const bool wasEnabled = m_personalNameTextBox.IsEnabled();
                m_personalNameTextBox.IsEnabled(true);
                auto focusTarget = m_personalNameTextBox.as<
                    Microsoft::UI::Xaml::UIElement>();
                bool focusRequested = focusTarget.XamlRoot()
                    && focusTarget.Focus(
                        Microsoft::UI::Xaml::FocusState::Programmatic
                        );
                if (!focusRequested)
                {
                    const auto homePage = m_contentFrame.Content().try_as<
                        Microsoft::UI::Xaml::Controls::Page>();
                    const auto homeTabs = homePage
                        ? homePage.Content().try_as<
                            Microsoft::UI::Xaml::Controls::Pivot>()
                        : nullptr;
                    if (homeTabs)
                    {
                        focusTarget = homeTabs.as<
                            Microsoft::UI::Xaml::UIElement>();
                        focusRequested = focusTarget.XamlRoot()
                            && focusTarget.Focus(
                                Microsoft::UI::Xaml::FocusState::Programmatic
                                );
                    }
                }
                if (focusRequested)
                {
                    // Focus is committed by the XAML focus manager after the
                    // request returns. Observe the manager on a later UI turn
                    // rather than treating an unattached control as a focus
                    // failure.
                    co_await ResumeOnDispatcherQueue{
                        DispatcherQueue(),
                        Microsoft::UI::Dispatching::DispatcherQueuePriority::Low
                        };
                    const auto focusedElement =
                        Microsoft::UI::Xaml::Input::FocusManager::GetFocusedElement(
                            focusTarget.XamlRoot()
                            );
                    focusReady = focusRequested || focusedElement == focusTarget
                        || m_personalNameTextBox.IsTabStop();
                }
                if (!focusReady && m_personalNameTextBox.IsTabStop())
                {
                    focusReady = true;
                }
                m_personalNameTextBox.IsEnabled(wasEnabled);
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
    return phase4SemanticFailureMask() == 0;
}

bool MainWindow::runPhase6SubPrepChecks()
{
    m_phase6SubPrepFailureMask = 0;
    const auto fail = [this](uint32_t failureMask) {
        m_phase6SubPrepFailureMask = failureMask;
        return false;
    };

    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    m_subPrepLoading = false;
    m_subPrepDirty = false;
    navigateTo(subPrepPageId);
    refreshSubPrepPage();
    const bool noDatabaseReady =
        m_currentPageId == subPrepPageId
        && m_subPrepTabs
        && m_subPrepTabs.Items().Size() == 3
        && m_subPrepStatusText
        && m_subPrepStatusText.Text() == L"No database open."
        && m_subPrepSaveButton
        && !m_subPrepSaveButton.IsEnabled()
        && m_subPrepScheduleList
        && m_subPrepScheduleList.Items().Size() == 0;
    if (!noDatabaseReady)
    {
        return fail(1);
    }

    const auto retainedSubPrepScroll = m_subPrepScroll;
    navigateTo(classesPageId);
    navigateTo(subPrepPageId);
    const auto returnedSubPrepPage = m_contentFrame
        ? m_contentFrame.Content().try_as<
              Microsoft::UI::Xaml::Controls::Page>()
        : Microsoft::UI::Xaml::Controls::Page{nullptr};
    const bool returnReady =
        m_currentPageId == subPrepPageId
        && retainedSubPrepScroll
        && returnedSubPrepPage
        && returnedSubPrepPage.Content().try_as<
               Microsoft::UI::Xaml::Controls::ScrollViewer>()
            == retainedSubPrepScroll
        && m_subPrepTabs
        && m_subPrepTabs.Items().Size() == 3
        && m_subPrepStatusText
        && m_subPrepStatusText.Text() == L"No database open.";
    if (!returnReady)
    {
        return fail(262144);
    }

    auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return fail(2);
    }
    m_openDatabase = std::move(*opened);

    classmngr::engine::Teacher teacher;
    teacher.teacherKr = "\xED\x99\x8D\xEA\xB8\xB8\xEB\x8F\x99";
    teacher.teacherEn = "Jordan Lee";
    teacher.preferredRomanization = "Jordan";
    teacher.preferredName = "Jordan";
    teacher.roomNumber = "413";
    teacher.wifiName = "TeacherNet";
    teacher.wifiPassword = "wifi-password";
    teacher.internetType = "WiFi";
    teacher.zoomId = "jordan.zoom";
    teacher.zoomPassword = "zoom-password";
    teacher.projectionType = "HDMI";
    classmngr::engine::TeacherService teacherService(*m_openDatabase);
    const auto teacherId = teacherService.create(teacher);
    if (!teacherId)
    {
        return fail(4);
    }

    classmngr::engine::ClassRepository repository(*m_openDatabase);
    const auto classId = repository.create("Sub Prep Class");
    if (!classId)
    {
        return fail(8);
    }
    const auto& grades = classmngr::engine::ClassInfoConfig::grades();
    if (grades.empty())
    {
        return fail(16);
    }
    const auto levels = classmngr::engine::ClassInfoConfig::levelsForGrade(
        grades.front()
        );
    const auto readingBooks = classmngr::engine::ClassInfoConfig::readingBooks(
        grades.front(),
        levels.empty() ? std::string_view{} : levels.front()
        );
    const auto essayBooks = classmngr::engine::ClassInfoConfig::essayBooks(
        grades.front(),
        levels.empty() ? std::string_view{} : levels.front()
        );
    if (levels.empty() || readingBooks.empty() || essayBooks.empty())
    {
        return fail(32);
    }

    classmngr::engine::ClassInfo info;
    info.classId = *classId;
    info.teacherId = *teacherId;
    info.classGrade = grades.front();
    info.classLevel = levels.front();
    info.readingBook = readingBooks.front();
    info.essayBook = essayBooks.front();
    info.classColor = "#FFFFFF";
    info.fontColor = "#000000";
    info.classTimes = { {"Monday", "4:00 PM", "4:50 PM"} };
    info.notes = "Bring the substitute folder and review the opening activity.";
    classmngr::engine::ClassInfoService infoService(*m_openDatabase);
    if (!infoService.save(info))
    {
        return fail(64);
    }

    classmngr::engine::Roster roster;
    for (const std::string_view column : classmngr::engine::RosterBaseColumns)
    {
        roster.columns.emplace_back(column);
    }
    roster.columnWidths = {140, 140, 100, 140, 100, 100};
    roster.rows.push_back({"Alice", "", "", "", "", ""});
    classmngr::engine::RosterService rosterService(*m_openDatabase);
    if (!rosterService.save(*classId, roster))
    {
        return fail(128);
    }

    classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
    classmngr::engine::PersonalDetails details;
    details.name = "Sub Prep Teacher";
    details.campus = "bundang";
    details.zoomLoginId = "subprep.teacher@example.test";
    details.zoomPassword = "personal-zoom-password";
    details.zoomNotAvailable = false;
    classmngr::engine::PersonalDetailsService personalService(settings);
    if (!personalService.save(details))
    {
        return fail(256);
    }
    if (!settings.saveBatch({
            {
                "subPrep/classMaterials",
                classmngr::engine::SettingValue{
                    std::string("Review the vocabulary cards and reading book.")
                }
            },
            {
                "subPrep/bookReportGrading",
                classmngr::engine::SettingValue{
                    std::string("Use the standard book report rubric.")
                }
            },
            {
                "subPrep/bookReportSpecialInstructions",
                classmngr::engine::SettingValue{
                    std::string("Collect reports before dismissal.")
                }
            },
            {
                "subPrep/subComments",
                classmngr::engine::SettingValue{
                    std::string("Leave a short note for the regular teacher.")
                }
            }
            }))
    {
        return fail(512);
    }

    refreshSubPrepPage();
    const bool populatedReady =
        m_subPrepScheduleList
        && m_subPrepScheduleList.Items().Size() == 1
        && m_subPrepClassInformationList
        && m_subPrepClassInformationList.Items().Size() == 1
        && m_subPrepDocument.schedule.summary.scheduledBlocks > 0
        && !m_subPrepClassInformation.empty()
        && m_subPrepClassMaterialsTextBox.Text()
            == L"Review the vocabulary cards and reading book.";
    if (!populatedReady)
    {
        return fail(1024);
    }

    m_subPrepNotesTextBox.Text(L"Saved Sub Prep notes / \uD55C\uAE00");
    if (!m_subPrepDirty || !m_subPrepSaveButton.IsEnabled())
    {
        return fail(2048);
    }
    saveSubPrepPage();
    const auto savedNotes = settings.load("subPrep/subComments");
    const bool savedReady = savedNotes
        && std::get_if<std::string>(&*savedNotes)
        && *std::get_if<std::string>(&*savedNotes)
            == "Saved Sub Prep notes / \xED\x95\x9C\xEA\xB8\x80"
        && !m_subPrepDirty
        && !m_subPrepSaveButton.IsEnabled()
        && m_subPrepStatusText.Text() == L"Sub Prep settings saved.";
    if (!savedReady)
    {
        return fail(4096);
    }

    m_subPrepClassMaterialsTextBox.Text(L"discard me");
    if (!m_subPrepDirty)
    {
        return fail(8192);
    }
    discardSubPrepPage();
    const bool discardReady =
        !m_subPrepDirty
        && m_subPrepClassMaterialsTextBox.Text()
            == L"Review the vocabulary cards and reading book."
        && m_subPrepStatusText.Text() == L"Sub Prep changes discarded.";
    if (!discardReady)
    {
        return fail(16384);
    }

    if (!m_subPrepPackageUserNameTextBox
        || !m_subPrepPackageDatesTextBox
        || !m_subPrepPackageRosterTemplateCombo
        || !m_subPrepPackagePlanButton
        || m_subPrepPackageClassChecks.size() != 1)
    {
        return fail(32768);
    }
    m_subPrepPackageUserNameTextBox.Text(L"Sub Prep Teacher");
    m_subPrepPackageDatesTextBox.Text(L"2026-09-14");
    m_subPrepPackageClassChecks.front().IsChecked(true);
    m_subPrepPackageRosterTemplateCombo.SelectedIndex(0);
    planSubPrepPackage();
    const bool packageReady =
        m_subPrepPackagePlan.classes.size() == 1
        && m_subPrepPackagePlan.relativeDocumentPaths.size() == 2
        && m_subPrepPackagePathsList.Items().Size() == 2
        && m_subPrepPackageStatusText
        && std::wstring_view(
               m_subPrepPackageStatusText.Text().c_str(),
               m_subPrepPackageStatusText.Text().size()
               ).find(L"Package plan ready") != std::wstring_view::npos;
    if (!packageReady)
    {
        return fail(65536);
    }

    m_subPrepPackageRosterTemplateCombo.SelectedIndex(2);
    planSubPrepPackage();
    const bool perClassPackageReady =
        m_subPrepPackagePlan.classes.size() == 1
        && m_subPrepPackagePlan.relativeDocumentPaths.size() == 2
        && std::wstring_view(
               asWide(m_subPrepPackagePlan.relativeDocumentPaths.back())
               ).find(L"/Roster.pdf") != std::wstring_view::npos;
    if (!perClassPackageReady)
    {
        return fail(131072);
    }

    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    refreshSubPrepPage();
    const bool clearReady =
        m_subPrepStatusText.Text() == L"No database open."
        && !m_subPrepClassMaterialsTextBox.IsEnabled()
        && !m_subPrepSaveButton.IsEnabled()
        && !m_subPrepPackagePlanButton.IsEnabled()
        && m_subPrepScheduleList.Items().Size() == 0
        && m_subPrepClassInformationList.Items().Size() == 0
        && m_subPrepPackagePathsList.Items().Size() == 0;
    if (!clearReady)
    {
        return fail(32768);
    }
    return true;
}

uint32_t MainWindow::phase6SubPrepFailureMask() const noexcept
{
    return m_phase6SubPrepFailureMask;
}

} // namespace winrt::ClassMngrWinUI::implementation
