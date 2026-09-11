#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

bool MainWindow::runPhase6ScheduleChecks()
{
    m_phase6ScheduleFailureMask = 0;
    const auto fail = [this](uint32_t failureMask) {
        m_phase6ScheduleFailureMask = failureMask;
        return false;
    };
    const auto selectClass = [this](int classId) {
        if (!m_scheduleClassSelector)
        {
            return false;
        }
        for (int index = 0;
             index < static_cast<int>(m_scheduleClassSelector.Items().Size());
             ++index)
        {
            const auto item = m_scheduleClassSelector.Items().GetAt(index)
                .try_as<Microsoft::UI::Xaml::Controls::ComboBoxItem>();
            if (item && boxedInt(item.Tag()) == classId)
            {
                m_scheduleClassSelector.SelectedIndex(index);
                return true;
            }
        }
        return false;
    };
    const auto selectRow = [this](int classId,
                                  classmngr::engine::ScheduleType type,
                                  std::wstring_view day) {
        if (!m_scheduleList)
        {
            return false;
        }
        for (int index = 0;
             index < static_cast<int>(m_scheduleList.Items().Size());
             ++index)
        {
            const auto item = m_scheduleList.Items().GetAt(index)
                .try_as<Microsoft::UI::Xaml::Controls::ListViewItem>();
            if (!item)
            {
                continue;
            }
            const auto selection = scheduleSelectionFromKey(boxedString(item.Tag()));
            if (selection && selection->classId == classId
                && selection->type == type && selection->day == day)
            {
                m_scheduleList.SelectedIndex(index);
                return true;
            }
        }
        return false;
    };

    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    m_scheduleLoading = false;
    m_scheduleEditingKey.clear();
    if (!ensureHomePage())
    {
        return fail(1);
    }
    refreshScheduleWorkspace();
    const bool noDatabaseReady =
        m_scheduleTabs && m_scheduleWorkspaceStatusText
        && m_scheduleWorkspaceStatusText.Text() == L"No database open."
        && m_scheduleList && !m_scheduleList.IsEnabled()
        && m_scheduleSaveButton && !m_scheduleSaveButton.IsEnabled();
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

    classmngr::engine::ClassRepository repository(*m_openDatabase);
    const auto scheduledClassId = repository.create("Schedule A");
    const auto conflictingClassId = repository.create("Schedule B");
    if (!scheduledClassId || !conflictingClassId)
    {
        return fail(4);
    }

    const auto& grades = classmngr::engine::ClassInfoConfig::grades();
    if (grades.empty())
    {
        return fail(8);
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
        return fail(8);
    }

    const auto createInfo = [&grades, &levels, &readingBooks, &essayBooks](
                                int classId,
                                std::vector<classmngr::engine::ClassTime> times
                                ) {
        classmngr::engine::ClassInfo info;
        info.classId = classId;
        info.classGrade = grades.front();
        info.classLevel = levels.front();
        info.readingBook = readingBooks.front();
        info.essayBook = essayBooks.front();
        info.classColor = "#FFFFFF";
        info.fontColor = "#000000";
        info.classTimes = std::move(times);
        return info;
    };
    classmngr::engine::ClassInfoService infoService(*m_openDatabase);
    if (!infoService.save(createInfo(*scheduledClassId, {}))
        || !infoService.save(createInfo(
            *conflictingClassId,
            { {"Monday", "5:00 PM", "5:55 PM"} }
            )))
    {
        return fail(16);
    }

    refreshScheduleWorkspace();
    const bool populatedReady =
        m_scheduleHeaderGrid && m_scheduleHeaderGrid.Children().Size() == 5
        && m_scheduleClassSelector
        && m_scheduleClassSelector.Items().Size() == 2
        && m_scheduleList && m_scheduleList.Items().Size() == 2;
    if (!populatedReady || !selectClass(*scheduledClassId))
    {
        return fail(32);
    }

    m_scheduleTypeCombo.SelectedIndex(0);
    m_scheduleDayCombo.SelectedIndex(0);
    m_scheduleStartTextBox.Text(L"4:00 PM");
    m_scheduleEndTextBox.Text(L"4:55 PM");
    saveScheduleEntry();
    auto regularInfo = infoService.load(*scheduledClassId);
    const bool regularSaved = regularInfo
        && regularInfo->classTimes.size() == 1
        && regularInfo->classTimes.front().day == "Monday";
    if (!regularSaved)
    {
        return fail(64);
    }

    const bool selectedRegular = selectRow(
        *scheduledClassId,
        classmngr::engine::ScheduleType::Regular,
        L"Monday"
        );
    if (!selectedRegular)
    {
        return fail(128);
    }
    m_scheduleStartTextBox.Text(L"5:15 PM");
    m_scheduleEndTextBox.Text(L"6:00 PM");
    saveScheduleEntry();
    regularInfo = infoService.load(*scheduledClassId);
    const bool conflictRejected =
        m_scheduleValidationText
        && m_scheduleValidationText.Visibility()
            == Microsoft::UI::Xaml::Visibility::Visible
        && regularInfo && regularInfo->classTimes.size() == 1
        && regularInfo->classTimes.front().startTime == "4:00 PM";
    if (!conflictRejected)
    {
        return fail(256);
    }

    m_scheduleStartTextBox.Text(L"4:00 PM");
    m_scheduleEndTextBox.Text(L"4:55 PM");
    saveScheduleEntry();
    m_scheduleTypeCombo.SelectedIndex(1);
    m_scheduleDayCombo.SelectedIndex(1);
    m_scheduleStartTextBox.Text(L"09:00");
    m_scheduleEndTextBox.Text(L"09:50");
    saveScheduleEntry();
    const auto savedWithIntensive = infoService.load(*scheduledClassId);
    const bool intensiveSaved = savedWithIntensive
        && savedWithIntensive->classTimes.size() == 1
        && savedWithIntensive->intensiveTimes.size() == 1
        && savedWithIntensive->intensiveTimes.front().day == "Tuesday";
    if (!intensiveSaved)
    {
        return fail(512);
    }

    const bool importControlsReady =
        m_scheduleImportKindCombo
        && m_scheduleImportUserTextBox
        && m_scheduleImportTeacherTextBox
        && m_scheduleImportGradeTextBox
        && m_scheduleImportLevelTextBox
        && m_scheduleImportRoomTextBox
        && m_scheduleImportDaysTextBox
        && m_scheduleImportStartTextBox
        && m_scheduleImportEndTextBox
        && m_scheduleImportTeacherActionCombo
        && m_scheduleImportClassActionCombo
        && m_scheduleImportStatusText
        && m_scheduleImportValidationText
        && m_scheduleImportPreviewButton
        && m_scheduleImportApplyButton;
    if (!importControlsReady)
    {
        return fail(2048);
    }

    m_scheduleImportKindCombo.SelectedIndex(0);
    m_scheduleImportUserTextBox.Text(L"WinUI Import User");
    m_scheduleImportTeacherTextBox.Text(L"\uD64D\uAE38\uB3D9");
    m_scheduleImportGradeTextBox.Text(L"E5");
    m_scheduleImportLevelTextBox.Text(L"Zeus");
    m_scheduleImportRoomTextBox.Text(L"413");
    m_scheduleImportDaysTextBox.Text(L"Monday, Wednesday");
    m_scheduleImportStartTextBox.Text(L"4:00 PM");
    m_scheduleImportEndTextBox.Text(L"4:55 PM");
    m_scheduleImportTeacherActionCombo.SelectedIndex(1);
    m_scheduleImportClassActionCombo.SelectedIndex(1);
    previewScheduleImport();
    const auto importStatus = m_scheduleImportStatusText.Text();
    const bool importPreviewReady =
        m_scheduleImportPreviewReady
        && m_scheduleImportPreview
        && m_scheduleImportPreview->user.classes.size() == 1
        && m_scheduleImportPreview->classes.size() == 1
        && m_scheduleImportPreview->teachers.size() == 1
        && m_scheduleImportApplyButton.IsEnabled()
        && std::wstring_view(importStatus.c_str(), importStatus.size()).find(
            L"Preview ready"
            ) != std::wstring_view::npos;
    if (!importPreviewReady)
    {
        return fail(2048);
    }

    applyScheduleImport();
    const auto importedClassrooms = repository.list();
    classmngr::engine::ClassScheduleService scheduleService(*m_openDatabase);
    const auto importedInfos = scheduleService.loadScheduleClassInfos();
    classmngr::engine::TeacherService teacherService(*m_openDatabase);
    const auto importedTeachers = teacherService.list();
    int importedClassId = -1;
    bool importedTimesReady = false;
    if (importedInfos)
    {
        for (const auto& info : *importedInfos)
        {
            if (info.classGrade == "E5" && info.classLevel == "Zeus"
                && info.classTimes.size() == 2
                && info.classTimes[0].day == "Monday"
                && info.classTimes[1].day == "Wednesday")
            {
                importedClassId = info.classId;
                importedTimesReady = true;
                break;
            }
        }
    }
    const auto appliedStatus = m_scheduleImportStatusText.Text();
    const bool importApplied =
        m_scheduleImportPreviewReady == false
        && !m_scheduleImportApplyButton.IsEnabled()
        && importedClassrooms && importedClassrooms->size() == 3
        && importedTeachers && importedTeachers->size() == 1
        && importedTimesReady && importedClassId > 0
        && std::wstring_view(appliedStatus.c_str(), appliedStatus.size()).find(
            L"Import applied atomically"
            ) != std::wstring_view::npos;
    if (!importApplied)
    {
        return fail(4096);
    }

    const bool testingControlsReady =
        m_testingClassSelector
        && m_testingClassNameTextBox
        && m_testingClassGradeTextBox
        && m_testingClassLevelTextBox
        && m_testingClassRoomTextBox
        && m_testingDayCombo
        && m_testingStartTextBox
        && m_testingReplaceExistingCheck
        && m_testingAssignmentList
        && m_testingStatusText
        && m_testingValidationText
        && m_testingCreateButton
        && m_testingAssignButton
        && m_testingDeleteAssignmentButton;
    if (!testingControlsReady)
    {
        return fail(8192);
    }

    const auto selectedTestingClassId = [this]() {
        if (!m_testingClassSelector)
        {
            return -1;
        }
        const auto item = m_testingClassSelector.SelectedItem().try_as<
            Microsoft::UI::Xaml::Controls::ComboBoxItem>();
        return item ? boxedInt(item.Tag()) : -1;
    };
    const auto selectTestingClass = [this](int classId) {
        if (!m_testingClassSelector)
        {
            return false;
        }
        for (int index = 0;
             index < static_cast<int>(m_testingClassSelector.Items().Size());
             ++index)
        {
            const auto item = m_testingClassSelector.Items().GetAt(index)
                .try_as<Microsoft::UI::Xaml::Controls::ComboBoxItem>();
            if (item && boxedInt(item.Tag()) == classId)
            {
                m_testingClassSelector.SelectedIndex(index);
                return true;
            }
        }
        return false;
    };

    m_testingClassNameTextBox.Text(L"WinUI Testing Group");
    m_testingClassGradeTextBox.Text(L"M1");
    m_testingClassLevelTextBox.Text(L"Mixed (All)");
    m_testingClassRoomTextBox.Text(L"Testing room 1");
    createTestingClass();
    const int firstTestingClassId = selectedTestingClassId();
    const bool firstTestingClassReady = firstTestingClassId > 0
        && m_testingClasses.size() == 1
        && m_testingClassSelector.Items().Size() == 1;
    if (!firstTestingClassReady)
    {
        return fail(8192);
    }

    m_testingClassNameTextBox.Text(L"WinUI Testing Group 2");
    m_testingClassRoomTextBox.Text(L"Testing room 2");
    createTestingClass();
    const int secondTestingClassId = selectedTestingClassId();
    if (secondTestingClassId <= 0 || secondTestingClassId == firstTestingClassId
        || m_testingClasses.size() != 2
        || m_testingClassSelector.Items().Size() != 2)
    {
        return fail(16384);
    }

    if (!selectTestingClass(firstTestingClassId))
    {
        return fail(32768);
    }
    m_testingDayCombo.SelectedIndex(0);
    m_testingStartTextBox.Text(L"09:00");
    m_testingReplaceExistingCheck.IsChecked(false);
    assignTestingClass();
    const bool assignmentCreated = m_testingAssignments.size() == 1
        && m_testingAssignments.front().classId == firstTestingClassId
        && m_testingAssignments.front().day == "Monday"
        && m_testingAssignments.front().startTime == "09:00";
    if (!assignmentCreated)
    {
        return fail(32768);
    }

    if (!selectTestingClass(secondTestingClassId))
    {
        return fail(65536);
    }
    m_testingReplaceExistingCheck.IsChecked(false);
    assignTestingClass();
    const bool replacementRejected =
        m_testingValidationText.Visibility()
            == Microsoft::UI::Xaml::Visibility::Visible
        && m_testingAssignments.size() == 1
        && m_testingAssignments.front().classId == firstTestingClassId;
    if (!replacementRejected)
    {
        return fail(65536);
    }

    m_testingReplaceExistingCheck.IsChecked(true);
    assignTestingClass();
    const bool assignmentReplaced = m_testingAssignments.size() == 1
        && m_testingAssignments.front().classId == secondTestingClassId
        && m_testingValidationText.Visibility()
            == Microsoft::UI::Xaml::Visibility::Collapsed;
    if (!assignmentReplaced)
    {
        return fail(131072);
    }

    m_testingAssignmentList.SelectedIndex(0);
    const bool deleteSelectionReady = m_testingDeleteAssignmentButton.IsEnabled();
    deleteTestingAssignment();
    const bool assignmentDeleted = deleteSelectionReady
        && m_testingAssignments.empty()
        && m_testingAssignmentList.Items().Size() == 0;
    if (!assignmentDeleted)
    {
        return fail(262144);
    }

    m_openDatabase.reset();
    refreshScheduleWorkspace();
    refreshTestingWorkspace();
    const bool clearedReady =
        m_scheduleWorkspaceStatusText.Text() == L"No database open."
        && !m_scheduleList.IsEnabled()
        && !m_scheduleClassSelector.IsEnabled()
        && m_testingStatusText.Text() == L"No database open."
        && !m_testingCreateButton.IsEnabled();
    return clearedReady ? true : fail(1024);
}

uint32_t MainWindow::phase6ScheduleFailureMask() const noexcept
{
    return m_phase6ScheduleFailureMask;
}

} // namespace winrt::ClassMngrWinUI::implementation
