#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

bool MainWindow::runPhase6ClassInformationChecks()
{
    m_phase6ClassInformationFailureMask = 0;
    const auto fail = [this](uint32_t failureMask) {
        m_phase6ClassInformationFailureMask = failureMask;
        return false;
    };
    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    m_classLoading = false;
    m_classDirty = false;
    m_classDetailsDirty = false;
    m_classNotesDirty = false;
    m_classNew = false;

    navigateTo(classesPageId);
    refreshClassesPage();
    const bool noDatabaseReady =
        m_currentPageId == classesPageId
        && classNavigationLocation() == ClassNavigationLocation::Top
        && m_classStatusText
        && m_classStatusText.Text() == L"No database open."
        && m_classNewButton
        && !m_classNewButton.IsEnabled()
        && m_classNotesStatusText.Text() == L"No database open.";
    if (!noDatabaseReady)
    {
        return fail(1);
    }

    auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return fail(2);
    }
    m_openDatabase = std::move(*opened);
    refreshClassesPage();
    const bool emptyReady =
        m_classSelector.Items().Size() == 0
        && m_classStatusText.Text()
            == L"No classes found. Choose New Class to add one."
        && m_classNewButton.IsEnabled();
    if (!emptyReady)
    {
        return fail(3);
    }

    const auto emptySettings =
        classmngr::engine::ApplicationSettingsService(*m_openDatabase)
            .load(classNavigationLocationKey);
    const bool defaultNavigationReady =
        emptySettings
        && std::holds_alternative<std::monostate>(*emptySettings)
        && classNavigationLocation() == ClassNavigationLocation::Top
        && m_classPageRoot
        && m_classPageRoot.RowDefinitions().Size() == 5
        && m_classSectionSelectorBar
        && m_classSectionSelectorBar.Items().Size() == 6
        && m_classSectionTitle
        && m_classSectionTitle.Text() == L"Class Details"
        && m_classSectionContentHost
        && m_classSectionContentHost.Content().try_as<
            Microsoft::UI::Xaml::Controls::ScrollViewer>()
        && m_classNavigationCard
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classSectionSelectorBar
            ) == 0
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classNavigationCard
            ) == 1
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classSectionTitle
            ) == 2
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classSectionContentHost
            ) == 3
        && m_classSectionActionsHost
        && m_classSectionActionsHost.Visibility()
            == Microsoft::UI::Xaml::Visibility::Collapsed
        && m_classPageRoot.RowDefinitions().GetAt(1).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Auto
        && m_classPageRoot.RowDefinitions().GetAt(2).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Auto
        && m_classPageRoot.RowDefinitions().GetAt(3).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Star;
    if (!defaultNavigationReady)
    {
        return fail(16);
    }

    ClassNewButton_Click(
        m_classNewButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    m_classNameTextBox.Text(L"Portable Class");
    m_classGradeCombo.SelectedIndex(1);
    m_classLevelCombo.SelectedIndex(1);
    m_classReadingBookCombo.SelectedIndex(2);
    m_classEssayBookCombo.SelectedIndex(2);
    m_classColorTextBox.Text(L"#AABBCC");
    m_classFontColorTextBox.Text(L"#102030");
    ClassSaveButton_Click(
        m_classSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    const bool createdReady =
        m_classSelector.Items().Size() == 1
        && m_classSelectedId > 0
        && !m_classDirty
        && m_classStatusText.Text() == L"Class information saved.";
    if (!createdReady)
    {
        return fail(4);
    }

    m_classNameTextBox.Text(L"Portable Class Updated");
    ClassSaveButton_Click(
        m_classSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    classmngr::engine::ClassRepository repository(*m_openDatabase);
    const auto listed = repository.list();
    const bool updatedReady = listed
        && listed->size() == 1
        && listed->front().name == "Portable Class Updated"
        && !m_classDirty;
    if (!updatedReady)
    {
        return fail(5);
    }

    m_classColorTextBox.Text(L"#not-a-color");
    ClassSaveButton_Click(
        m_classSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    const bool invalidReady =
        m_classDirty
        && m_classValidationText.Visibility()
            == Microsoft::UI::Xaml::Visibility::Visible;
    ClassDiscardButton_Click(
        m_classDiscardButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    if (!invalidReady || m_classDirty)
    {
        return fail(6);
    }

    m_classNotesTextBox.Text(L"Notes from WinUI / \uD55C\uAE00");
    m_classTimeFillerActivitiesTextBox.Text(L"Vocabulary review");
    ClassNotesSaveButton_Click(
        m_classNotesSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    classmngr::engine::ClassInfoService classInfo(*m_openDatabase);
    const auto withNotes = classInfo.load(m_classSelectedId);
    if (!withNotes)
    {
        return fail(7);
    }
    if (withNotes->notes != "Notes from WinUI / \xED\x95\x9C\xEA\xB8\x80")
    {
        return fail(12);
    }
    if (withNotes->timeFillerActivities != "Vocabulary review")
    {
        return fail(13);
    }
    if (m_classDirty)
    {
        return fail(14);
    }

    const auto secondId = repository.create("Second Class");
    if (!secondId)
    {
        return fail(8);
    }

    auto firstNavigationInfo = *withNotes;
    firstNavigationInfo.classId = m_classSelectedId;
    firstNavigationInfo.classTimes = {
        {"Monday", "4:00 PM", "4:50 PM"}
    };
    if (!classInfo.save(firstNavigationInfo))
    {
        return fail(32);
    }
    auto secondNavigationInfo = firstNavigationInfo;
    secondNavigationInfo.classId = *secondId;
    secondNavigationInfo.classTimes = {
        {"Tuesday", "5:00 PM", "5:50 PM"}
    };
    if (!classInfo.save(secondNavigationInfo))
    {
        return fail(64);
    }

    refreshClassesPage();
    if (m_classSelector.Items().Size() != 2)
    {
        return fail(9);
    }

    m_classSectionSelectorBar.SelectedItem(m_classSectionSelectorItems[5]);
    const bool sectionSelectionReady =
        m_classSectionIndex == 5
        && m_classSectionSelectorBar.SelectedItem()
            == m_classSectionSelectorItems[5]
        && m_classSectionTitle.Text() == L"Class Notes"
        && m_classSectionContentHost.Content().try_as<
            Microsoft::UI::Xaml::Controls::ScrollViewer>();
    if (!sectionSelectionReady)
    {
        return fail(128);
    }

    m_classSectionSelectorBar.SelectedItem(m_classSectionSelectorItems[1]);
    const bool rosterActionsReady =
        m_classSectionTitle.Text() == L"Class Roster"
        && m_classSectionActionsHost.Visibility()
            == Microsoft::UI::Xaml::Visibility::Visible
        && m_classSectionActionsHost.Content().try_as<
            Microsoft::UI::Xaml::Controls::Grid>()
            == m_classRosterActions;
    if (!rosterActionsReady)
    {
        return fail(65536);
    }
    m_classSectionSelectorBar.SelectedItem(m_classSectionSelectorItems[3]);
    const bool evaluationActionsReady =
        m_classSectionTitle.Text() == L"Speaking Evaluations"
        && m_classSectionActionsHost.Visibility()
            == Microsoft::UI::Xaml::Visibility::Visible
        && m_classSectionActionsHost.Content().try_as<
            Microsoft::UI::Xaml::Controls::Grid>()
            == m_speakingEvaluationActions;
    if (!evaluationActionsReady)
    {
        return fail(131072);
    }
    m_classSectionSelectorBar.SelectedItem(m_classSectionSelectorItems[5]);

    selectClassNavigationGrade(false, firstNavigationInfo.classGrade);
    toggleClassNavigationDay("Monday");
    const bool topNavigationReady =
        classNavigationLocation() == ClassNavigationLocation::Top
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classNavigationCard
            ) == 1
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classSectionTitle
            ) == 2
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classSectionContentHost
            ) == 3
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classSectionActionsHost
            ) == 4
        && m_classPageRoot.RowDefinitions().GetAt(1).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Auto
        && m_classPageRoot.RowDefinitions().GetAt(2).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Auto
        && m_classPageRoot.RowDefinitions().GetAt(3).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Star
        && !m_classNavigationAll
        && std::find(
            m_classNavigationSelectedDays.begin(),
            m_classNavigationSelectedDays.end(),
            "Monday"
            ) != m_classNavigationSelectedDays.end()
        && m_classNavigationClassTabs.Children().Size() == 1;
    if (!topNavigationReady)
    {
        return fail(256);
    }

    const int firstClassId = m_classSelectedId;
    selectClassFromNavigation(*secondId);
    const bool secondClassSelected = m_classSelectedId == *secondId;
    selectClassFromNavigation(firstClassId);
    const bool firstClassSelected = m_classSelectedId == firstClassId;
    if (!secondClassSelected || !firstClassSelected)
    {
        return fail(512);
    }

    setClassNavigationLocation(ClassNavigationLocation::Bottom);
    const auto savedNavigationLocation =
        classmngr::engine::ApplicationSettingsService(*m_openDatabase)
            .load(classNavigationLocationKey);
    const bool bottomNavigationReady =
        classNavigationLocation() == ClassNavigationLocation::Bottom
        && savedNavigationLocation
        && std::get_if<std::string>(&*savedNavigationLocation)
            != nullptr
        && *std::get_if<std::string>(&*savedNavigationLocation) == "bottom"
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classNavigationCard
            ) == 2
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classSectionTitle
            ) == 3
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classSectionContentHost
            ) == 1
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classSectionActionsHost
            ) == 4
        && m_classPageRoot.RowDefinitions().GetAt(1).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Star
        && m_classPageRoot.RowDefinitions().GetAt(2).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Auto
        && m_classPageRoot.RowDefinitions().GetAt(3).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Auto
        && m_classSelectedId == firstClassId
        && m_classSectionIndex == 5
        && !m_classNavigationAll
        && std::find(
            m_classNavigationSelectedDays.begin(),
            m_classNavigationSelectedDays.end(),
            "Monday"
            ) != m_classNavigationSelectedDays.end();
    if (!bottomNavigationReady)
    {
        return fail(1024);
    }

    m_classNameTextBox.Text(L"Unsaved Location Change");
    const bool dirtyBeforeLocationToggle = m_classDirty;
    setClassNavigationLocation(ClassNavigationLocation::Top);
    const bool statePreservedByToggle =
        dirtyBeforeLocationToggle
        && m_classDirty
        && m_classSelectedId == firstClassId
        && m_classSectionIndex == 5
        && !m_classNavigationAll
        && std::find(
            m_classNavigationSelectedDays.begin(),
            m_classNavigationSelectedDays.end(),
            "Monday"
            ) != m_classNavigationSelectedDays.end();
    if (!statePreservedByToggle)
    {
        return fail(2048);
    }
    ClassDiscardButton_Click(
        m_classDiscardButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );

    setClassNavigationLocation(ClassNavigationLocation::Bottom);
    refreshClassesPage();
    const bool refreshPreservedNavigation =
        classNavigationLocation() == ClassNavigationLocation::Bottom
        && m_classSelectedId == firstClassId
        && m_classSectionIndex == 5
        && !m_classNavigationAll
        && std::find(
            m_classNavigationSelectedDays.begin(),
            m_classNavigationSelectedDays.end(),
            "Monday"
            ) != m_classNavigationSelectedDays.end()
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classNavigationCard
            ) == 2
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classSectionTitle
            ) == 3
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classSectionContentHost
            ) == 1
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classSectionActionsHost
            ) == 4
        && m_classPageRoot.RowDefinitions().GetAt(1).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Star
        && m_classPageRoot.RowDefinitions().GetAt(2).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Auto
        && m_classPageRoot.RowDefinitions().GetAt(3).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Auto;
    if (!refreshPreservedNavigation)
    {
        return fail(4096);
    }

    classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
    if (!settings.save(
            classNavigationLocationKey,
            classmngr::engine::SettingValue{std::string("invalid")}
            ))
    {
        return fail(8192);
    }
    refreshClassesPage();
    const bool invalidSettingReady =
        classNavigationLocation() == ClassNavigationLocation::Top
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classNavigationCard
            ) == 1
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classSectionTitle
            ) == 2
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classSectionContentHost
            ) == 3
        && m_classPageRoot.RowDefinitions().GetAt(1).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Auto
        && m_classPageRoot.RowDefinitions().GetAt(2).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Auto
        && m_classPageRoot.RowDefinitions().GetAt(3).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Star;
    if (!invalidSettingReady)
    {
        return fail(16384);
    }

    m_classNameTextBox.Text(L"Unsaved Class Name");
    const int selectedBefore = m_classSelectedId;
    const int otherIndex = m_classSelectedIndex == 0 ? 1 : 0;
    m_classSelector.SelectedIndex(otherIndex);
    const bool dirtySelectionProtected =
        m_classSelectedId == selectedBefore
        && m_classSelector.SelectedIndex() == m_classSelectedIndex
        && m_classDirty;
    ClassDiscardButton_Click(
        m_classDiscardButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    if (!dirtySelectionProtected)
    {
        return fail(10);
    }

    m_openDatabase.reset();
    refreshClassesPage();
    const bool clearReady = m_classStatusText.Text() == L"No database open."
        && classNavigationLocation() == ClassNavigationLocation::Top
        && !m_classNameTextBox.IsEnabled()
        && !m_classSaveButton.IsEnabled()
        && !m_classNotesSaveButton.IsEnabled();
    if (!clearReady)
    {
        return fail(11);
    }
    return true;
}

uint32_t MainWindow::phase6ClassInformationFailureMask() const noexcept
{
    return m_phase6ClassInformationFailureMask;
}

} // namespace winrt::ClassMngrWinUI::implementation
