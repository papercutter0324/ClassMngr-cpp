#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

uint32_t MainWindow::phase4SemanticFailureMask()
{
    if (!ensureHomePage() || !m_scheduleSlotTextBox || !m_scheduleStatusText)
    {
        return 1;
    }

    // Roster and speaking prototypes live on the Classes presentation now,
    // but the phase hook still exercises their controls directly.
    if (!m_rosterSourceList || !m_rosterTransferredList || !m_rosterStatusText
        || !m_speakingPasteTextBox || !m_speakingStatusText
        || m_speakingScoreCells.size() != 9)
    {
        navigateTo(classesPageId);
    }
    if (!m_rosterSourceList || !m_rosterTransferredList || !m_rosterStatusText
        || !m_speakingPasteTextBox || !m_speakingStatusText
        || m_speakingScoreCells.size() != 9)
    {
        return 1;
    }

    const auto contains = [](winrt::hstring const& value, std::wstring_view text) {
        return std::wstring_view(value.c_str(), value.size()).find(text)
            != std::wstring_view::npos;
    };
    const auto eventArguments = Microsoft::UI::Xaml::RoutedEventArgs();
    m_scheduleSlotTextBox.Text(L"09:45â€“10:30");
    ScheduleApplyButton_Click(nullptr, eventArguments);
    const auto scheduleStatus = m_scheduleStatusText.Text();
    const bool scheduleReady = contains(scheduleStatus, L"not persisted");

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
    const bool firstScoreReady = m_speakingScoreCells[0].Text() == L"8";
    const bool secondScoreReady = m_speakingScoreCells[1].Text() == L"7";
    const bool sixthScoreReady = m_speakingScoreCells[5].Text() == L"8";
    const auto pasteStatus = m_speakingStatusText.Text();
    const bool pasteStatusReady = contains(pasteStatus, L"Applied 6");

    SpeakingAnalyticsButton_Click(nullptr, eventArguments);
    const auto analyticsStatus = m_speakingStatusText.Text();
    const bool analyticsReady = contains(
        analyticsStatus,
        L"Analytics navigation requested"
        );

    uint32_t failureMask = 0;
    failureMask |= scheduleReady ? 0 : 2;
    failureMask |= rosterReady ? 0 : 4;
    failureMask |= firstScoreReady ? 0 : 8;
    failureMask |= secondScoreReady ? 0 : 16;
    failureMask |= sixthScoreReady ? 0 : 32;
    failureMask |= pasteStatusReady ? 0 : 64;
    failureMask |= analyticsReady ? 0 : 128;
    failureMask |= m_dirtyState.isDirty() ? 0 : 256;
    return failureMask;
}

bool MainWindow::runPhase5CampusChecks()
{
    // Phase activation tests are shell-only, so this in-memory owner cannot
    // replace or observe a user database. Start by checking the explicit
    // no-database state, then seed the same service used by the page.
    m_phase5CampusScenario = L"no-database";
    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    navigateTo(campusInformationPageId);
    refreshCampusInformationPage();
    const bool noDatabaseReady = m_currentPageId == campusInformationPageId
        && m_campusInformationState == L"no_database"
        && static_cast<bool>(m_contentFrame.Content());

    auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return false;
    }

    auto& database = **opened;
    classmngr::engine::CampusRecordService service(database);
    const auto emptyResult = service.list();
    if (!emptyResult || !emptyResult->empty())
    {
        return false;
    }

    // Reproduce the user flow that previously left a cached no-database page
    // visible: leave Campus Information, open the database, then navigate
    // back to the cached page.
    m_phase5CampusScenario = L"empty";
    navigateTo(homePageId);
    m_openDatabase = std::move(*opened);
    navigateTo(campusInformationPageId);
    const bool emptyReady = m_campusInformationState == L"empty"
        && !m_campusSelector
        && static_cast<bool>(m_contentFrame.Content());
    if (!emptyReady)
    {
        m_openDatabase.reset();
        refreshCampusInformationPage();
        return false;
    }

    classmngr::engine::CampusRecordService populatedService(*m_openDatabase);
    classmngr::engine::CampusRecord campus;
    campus.name = winrt::to_string(winrt::hstring(L"ì„œìš¸ ìº í¼ìŠ¤"));
    campus.buildingName = winrt::to_string(winrt::hstring(L"ë³¸ê´€"));
    campus.address = winrt::to_string(winrt::hstring(L"ì„œìš¸íŠ¹ë³„ì‹œ ê°•ë‚¨êµ¬"));
    campus.phoneNumber = "+82-2-1234-5678";
    campus.officeNumber = winrt::to_string(winrt::hstring(L"ì‚¬ë¬´ì‹¤ 101í˜¸"));
    campus.transitSteps = winrt::to_string(winrt::hstring(L"2í˜¸ì„ ì—ì„œ í•˜ì°¨"));
    campus.arrivalInfo = winrt::to_string(winrt::hstring(L"ì•ˆë‚´ ë°ìŠ¤í¬ë¡œ ì˜¤ì„¸ìš”"));
    campus.imagePath = ":/assets/campuses/bundang/bundang_map.png";
    campus.officeWifi = "TeacherNet";
    campus.officeWifiPassword = "password";
    campus.printerName = "Printer-1";
    campus.printerSteps = "Load paper, then print.";
    campus.photocopierCode = "42";
    campus.housingLocations = winrt::to_string(winrt::hstring(L"ê°•ë‚¨, ì„œì´ˆ"));
    const auto created = populatedService.create(campus);
    if (!created)
    {
        m_openDatabase.reset();
        refreshCampusInformationPage();
        return false;
    }

    m_phase5CampusScenario = L"populated";
    refreshCampusInformationPage();
    if (!m_campusSelector || m_campusInformationState != L"populated"
        || m_campusSelector.Items().Size() != 1)
    {
        m_openDatabase.reset();
        refreshCampusInformationPage();
        return false;
    }

    m_campusSelector.SelectedIndex(0);
    presentSelectedCampus(m_currentPageId);
    const bool imageControlReady = static_cast<bool>(m_campusImage);
    bool koreanTextReady = false;
    for (uint32_t index = 0; index < m_campusDetailsPanel.Children().Size(); ++index)
    {
        const auto text = m_campusDetailsPanel.Children().GetAt(index).try_as<
            Microsoft::UI::Xaml::Controls::TextBlock>();
        if (text
            && std::wstring_view(text.Text().c_str(), text.Text().size()).find(
                L"ì„œìš¸ ìº í¼ìŠ¤"
                ) != std::wstring_view::npos)
        {
            koreanTextReady = true;
            break;
        }
    }

    const WinUILocalizer korean(L"ko-KR");
    const bool localizationReady = korean.hasString(
        L"CampusInformationPage",
        L"Campus Information"
        )
        && korean.getString(L"CampusInformationPage", L"Campus Information")
            == L"\xCEA0\xD37C\xC2A4 \xC815\xBCF4"
        && korean.getString(L"CampusInformationPage", L"Name")
            == L"\xC774\xB984";

    classmngr::windows::winui::WindowsResourceProvider resourceProvider;
    const auto imageExists = resourceProvider.exists(
        ":/assets/campuses/bundang/bundang_map.png"
        );
    const auto imageBytes = resourceProvider.readBytes(
        ":/assets/campuses/bundang/bundang_map.png"
        );
    const bool resourceReady = imageExists && *imageExists
        && imageBytes && !imageBytes->empty();

    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_phase5CampusScenario = L"no-database";
    refreshCampusInformationPage();
    const bool resetReady = m_campusInformationState == L"no_database"
        && !m_campusSelector;
    m_phase5CampusScenario.clear();
    return noDatabaseReady && emptyReady && koreanTextReady
        && imageControlReady && localizationReady && resourceReady
        && resetReady;
}

bool MainWindow::runPhase6PersonalDetailsChecks()
{
    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    m_personalDetailsDirty = false;

    navigateTo(personalDetailsPageId);
    refreshPersonalDetailsPage();
    const bool noDatabaseReady =
        m_currentPageId == homePageId
        && m_personalStatusText
        && m_personalStatusText.Text() == L"No database open."
        && m_personalSaveButton
        && !m_personalSaveButton.IsEnabled();

    auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
    if (!noDatabaseReady || !opened || *opened == nullptr)
    {
        return false;
    }

    m_openDatabase = std::move(*opened);
    refreshPersonalDetailsPage();
    if (!m_personalDetailsLoaded || !m_personalNameTextBox)
    {
        return false;
    }

    m_personalNameTextBox.Text(L"í™ê¸¸ë™");
    auto phase6Campus = Microsoft::UI::Xaml::Controls::ComboBoxItem();
    phase6Campus.Content(box_value(hstring(L"ì„œìš¸ ìº í¼ìŠ¤")));
    phase6Campus.Tag(box_value(hstring(L"ì„œìš¸ ìº í¼ìŠ¤")));
    m_personalCampusCombo.Items().Append(phase6Campus);
    m_personalCampusCombo.SelectedIndex(
        static_cast<int>(m_personalCampusCombo.Items().Size()) - 1
        );
    m_personalZoomNotAvailableCheck.IsChecked(false);
    m_personalZoomLoginIdTextBox.Text(L"teacher@example.test");
    m_personalZoomPasswordBox.Password(L"ë¹„ë°€ë²ˆí˜¸");
    m_personalSignatureModeCombo.SelectedIndex(1);
    m_personalTypedSignatureTextBox.Text(L"í™ê¸¸ë™ ì„œëª…");
    m_personalSignatureFontCombo.SelectedIndex(2);
    PersonalDetailsSaveButton_Click(
        m_personalSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    if (m_personalDetailsDirty
        || m_personalStatusText.Text() != L"Personal details saved.")
    {
        return false;
    }

    refreshPersonalDetailsPage();
    const bool roundTripReady =
        asWString(m_personalNameTextBox.Text()) == L"í™ê¸¸ë™"
        && selectedComboValue(m_personalCampusCombo) == L"ì„œìš¸ ìº í¼ìŠ¤"
        && asWString(m_personalZoomLoginIdTextBox.Text())
            == L"teacher@example.test"
        && asWString(m_personalZoomPasswordBox.Password()) == L"ë¹„ë°€ë²ˆí˜¸"
        && m_personalSignatureModeCombo.SelectedIndex() == 1
        && asWString(m_personalTypedSignatureTextBox.Text())
            == L"í™ê¸¸ë™ ì„œëª…"
        && m_personalSignatureFontCombo.SelectedIndex() == 2;

    m_openDatabase.reset();
    refreshPersonalDetailsPage();
    const bool clearReady =
        m_personalStatusText.Text() == L"No database open."
        && !m_personalNameTextBox.IsEnabled()
        && !m_personalSaveButton.IsEnabled();
    return roundTripReady && clearReady;
}

bool MainWindow::runPhase6KoreanTeacherChecks()
{
    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    m_koreanTeacherDirty = false;
    m_koreanTeacherNew = false;

    navigateTo(koreanTeachersPageId);
    const bool noDatabaseReady =
        m_currentPageId == koreanTeachersPageId
        && m_koreanTeacherStatusText
        && m_koreanTeacherStatusText.Text() == L"No database open."
        && m_koreanTeacherNewButton
        && !m_koreanTeacherNewButton.IsEnabled();

    auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
    if (!noDatabaseReady || !opened || *opened == nullptr)
    {
        return false;
    }

    m_openDatabase = std::move(*opened);
    refreshKoreanTeachersPage();
    const bool emptyReady =
        m_koreanTeacherSelector
        && m_koreanTeacherSelector.Items().Size() == 0
        && m_koreanTeacherStatusText.Text()
            == L"No Korean teachers found. Choose New Teacher to add one.";
    if (!emptyReady)
    {
        return false;
    }

    KoreanTeacherNewButton_Click(
        m_koreanTeacherNewButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    m_koreanTeacherKrTextBox.Text(L"\uAE40\uBBFC\uC11C");
    m_koreanTeacherEnTextBox.Text(L"Minseo Kim");
    m_koreanTeacherRomanizationTextBox.Text(L"Minseo");
    m_koreanTeacherPreferredNameCombo.SelectedIndex(0);
    m_koreanTeacherBirthdayTextBox.Text(L"03-14");
    m_koreanTeacherPhoneTextBox.Text(L"010-1234-5678");
    m_koreanTeacherNotesTextBox.Text(L"\uC11C\uC6B8 \uCF54\uB514\uB124\uC774\uD130");
    KoreanTeacherSaveButton_Click(
        m_koreanTeacherSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    const bool createdReady =
        m_koreanTeacherSelector.Items().Size() == 1
        && !m_koreanTeacherDirty
        && m_koreanTeacherStatusText.Text() == L"Korean teacher saved.";
    if (!createdReady)
    {
        return false;
    }

    m_koreanTeacherBirthdayTextBox.Text(L"13-40");
    KoreanTeacherSaveButton_Click(
        m_koreanTeacherSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    const bool invalidBirthdayReady =
        m_koreanTeacherDirty
        && m_koreanTeacherValidationText.Visibility()
            == Microsoft::UI::Xaml::Visibility::Visible;
    KoreanTeacherDiscardButton_Click(
        m_koreanTeacherDiscardButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    if (!invalidBirthdayReady || m_koreanTeacherDirty)
    {
        return false;
    }

    m_koreanTeacherKrTextBox.Text(L"\uD64D\uAE38\uB3D9");
    KoreanTeacherSaveButton_Click(
        m_koreanTeacherSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    classmngr::engine::TeacherService service(*m_openDatabase);
    const auto listed = service.list();
    const bool updatedReady = listed
        && listed->size() == 1
        && listed->front().teacherKr == "\xED\x99\x8D\xEA\xB8\xB8\xEB\x8F\x99"
        && !m_koreanTeacherDirty;

    m_openDatabase.reset();
    refreshKoreanTeachersPage();
    const bool clearReady =
        m_koreanTeacherStatusText.Text() == L"No database open."
        && !m_koreanTeacherKrTextBox.IsEnabled()
        && !m_koreanTeacherSaveButton.IsEnabled();
    return updatedReady && clearReady;
}

bool MainWindow::runPhase6NativeEnglishTeacherChecks()
{
    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    m_nativeEnglishTeacherDirty = false;
    m_nativeEnglishTeacherNew = false;

    navigateTo(nativeEnglishTeachersPageId);
    const bool noDatabaseReady =
        m_currentPageId == nativeEnglishTeachersPageId
        && m_nativeEnglishTeacherStatusText
        && m_nativeEnglishTeacherStatusText.Text() == L"No database open."
        && m_nativeEnglishTeacherNewButton
        && !m_nativeEnglishTeacherNewButton.IsEnabled();

    auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
    if (!noDatabaseReady || !opened || *opened == nullptr)
    {
        return false;
    }

    m_openDatabase = std::move(*opened);
    refreshNativeEnglishTeachersPage();
    const bool emptyReady =
        m_nativeEnglishTeacherSelector
        && m_nativeEnglishTeacherSelector.Items().Size() == 0
        && m_nativeEnglishTeacherStatusText.Text()
            == L"No Native English Teachers found. Choose New Teacher to add one.";
    if (!emptyReady)
    {
        return false;
    }

    NativeEnglishTeacherNewButton_Click(
        m_nativeEnglishTeacherNewButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    m_nativeEnglishTeacherNameTextBox.Text(L"Alice Smith");
    m_nativeEnglishTeacherPositionCombo.SelectedIndex(1);
    m_nativeEnglishTeacherPhoneTextBox.Text(L"010-5555-0101");
    m_nativeEnglishTeacherEmailTextBox.Text(L"alice@example.test");
    m_nativeEnglishTeacherBirthdayTextBox.Text(L"03-14");
    m_nativeEnglishTeacherNationalityTextBox.Text(L"Canadian");
    NativeEnglishTeacherSaveButton_Click(
        m_nativeEnglishTeacherSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    const bool createdReady =
        m_nativeEnglishTeacherSelector.Items().Size() == 1
        && !m_nativeEnglishTeacherDirty
        && m_nativeEnglishTeacherStatusText.Text()
            == L"Native English Teacher saved.";
    if (!createdReady)
    {
        return false;
    }

    m_nativeEnglishTeacherBirthdayTextBox.Text(L"13-40");
    NativeEnglishTeacherSaveButton_Click(
        m_nativeEnglishTeacherSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    const bool invalidBirthdayReady =
        m_nativeEnglishTeacherDirty
        && m_nativeEnglishTeacherValidationText.Visibility()
            == Microsoft::UI::Xaml::Visibility::Visible;
    NativeEnglishTeacherDiscardButton_Click(
        m_nativeEnglishTeacherDiscardButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    if (!invalidBirthdayReady || m_nativeEnglishTeacherDirty)
    {
        return false;
    }

    m_nativeEnglishTeacherNameTextBox.Text(L"Alice Cooper");
    NativeEnglishTeacherSaveButton_Click(
        m_nativeEnglishTeacherSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    classmngr::engine::NativeEnglishTeacherService service(*m_openDatabase);
    const auto listed = service.list();
    const bool updatedReady = listed
        && listed->size() == 1
        && listed->front().name == "Alice Cooper"
        && listed->front().email == "alice@example.test"
        && !m_nativeEnglishTeacherDirty;

    m_openDatabase.reset();
    refreshNativeEnglishTeachersPage();
    const bool clearReady =
        m_nativeEnglishTeacherStatusText.Text() == L"No database open."
        && !m_nativeEnglishTeacherNameTextBox.IsEnabled()
        && !m_nativeEnglishTeacherSaveButton.IsEnabled();
    return updatedReady && clearReady;
}

bool MainWindow::runPhase6GsTeamChecks()
{
    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    m_gsTeamDirty = false;
    m_gsTeamNew = false;

    navigateTo(gsTeamPageId);
    const bool noDatabaseReady =
        m_currentPageId == gsTeamPageId
        && m_gsTeamStatusText
        && m_gsTeamStatusText.Text() == L"No database open."
        && m_gsTeamNewButton
        && !m_gsTeamNewButton.IsEnabled();

    auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
    if (!noDatabaseReady || !opened || *opened == nullptr)
    {
        return false;
    }

    m_openDatabase = std::move(*opened);
    refreshGsTeamPage();
    const bool emptyReady =
        m_gsTeamSelector
        && m_gsTeamSelector.Items().Size() == 0
        && m_gsTeamStatusText.Text()
            == L"No GS Team members found. Choose New Member to add one.";
    if (!emptyReady)
    {
        return false;
    }

    GsTeamNewButton_Click(
        m_gsTeamNewButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    m_gsTeamNameTextBox.Text(L"Jane Doe");
    m_gsTeamKoreanNameTextBox.Text(L"\uC81C\uC778 \uB450");
    m_gsTeamPositionTextBox.Text(L"Branch Manager");
    m_gsTeamPhoneTextBox.Text(L"010-5555-0202");
    m_gsTeamBirthdayTextBox.Text(L"04-21");
    GsTeamSaveButton_Click(
        m_gsTeamSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    const bool createdReady =
        m_gsTeamSelector.Items().Size() == 1
        && !m_gsTeamDirty
        && m_gsTeamStatusText.Text() == L"GS Team member saved.";
    if (!createdReady)
    {
        return false;
    }

    m_gsTeamBirthdayTextBox.Text(L"13-40");
    GsTeamSaveButton_Click(
        m_gsTeamSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    const bool invalidBirthdayReady =
        m_gsTeamDirty
        && m_gsTeamValidationText.Visibility()
            == Microsoft::UI::Xaml::Visibility::Visible;
    GsTeamDiscardButton_Click(
        m_gsTeamDiscardButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    if (!invalidBirthdayReady || m_gsTeamDirty)
    {
        return false;
    }

    m_gsTeamKoreanNameTextBox.Text(L"\uC81C\uC778 \uCFE0\uD37C");
    GsTeamSaveButton_Click(
        m_gsTeamSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    classmngr::engine::GsTeamService service(*m_openDatabase);
    const auto listed = service.list();
    const bool updatedReady = listed
        && listed->size() == 1
        && listed->front().name == "Jane Doe"
        && listed->front().koreanName == "\xEC\xA0\x9C\xEC\x9D\xB8 \xEC\xBF\xA0\xED\x8D\xBC"
        && !m_gsTeamDirty;

    m_openDatabase.reset();
    refreshGsTeamPage();
    const bool clearReady =
        m_gsTeamStatusText.Text() == L"No database open."
        && !m_gsTeamNameTextBox.IsEnabled()
        && !m_gsTeamSaveButton.IsEnabled();
    return updatedReady && clearReady;
}

} // namespace winrt::ClassMngrWinUI::implementation
