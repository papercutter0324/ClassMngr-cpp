#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

namespace
{
std::wstring testingClassLabel(
    classmngr::engine::TestingClass const& testingClass
    )
{
    return asWide(testingClass.name) + L" \u2014 "
        + asWide(testingClass.grade) + L" \u2014 "
        + asWide(testingClass.level);
}

void selectTestingComboValue(
    Microsoft::UI::Xaml::Controls::ComboBox const& combo,
    std::wstring_view value
    )
{
    if (!combo)
    {
        return;
    }

    for (int index = 0;
         index < static_cast<int>(combo.Items().Size());
         ++index)
    {
        const auto item = combo.Items().GetAt(index).try_as<
            Microsoft::UI::Xaml::Controls::ComboBoxItem>();
        if (item && boxedString(item.Tag()) == value)
        {
            combo.SelectedIndex(index);
            return;
        }
    }
    combo.SelectedIndex(-1);
}

void setTestingPreviewColor(
    Microsoft::UI::Xaml::Controls::Border const& preview,
    std::string_view value
    )
{
    if (!preview)
    {
        return;
    }

    preview.Background(
        Microsoft::UI::Xaml::Media::SolidColorBrush(
            uiColorFromHex(value)
            )
        );
}
} // namespace

void MainWindow::refreshTestingWorkspace()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_testingClassSelector || !m_testingAssignmentList
        || !m_testingStatusText || !m_testingClassList)
    {
        return;
    }

    const bool hasDatabase = static_cast<bool>(m_openDatabase);
    const auto setEnabled = [hasDatabase](auto const& control) {
        if (control)
        {
            control.IsEnabled(hasDatabase);
        }
    };
    setEnabled(m_testingClassSelector);
    setEnabled(m_testingClassNameTextBox);
    setEnabled(m_testingClassGradeTextBox);
    setEnabled(m_testingClassLevelTextBox);
    setEnabled(m_testingClassRoomTextBox);
    setEnabled(m_testingClassList);
    setEnabled(m_testingClassTeacherCombo);
    setEnabled(m_testingClassGradeCombo);
    setEnabled(m_testingClassLevelCombo);
    setEnabled(m_testingClassNotesTextBox);
    setEnabled(m_testingDayCombo);
    setEnabled(m_testingStartTextBox);
    setEnabled(m_testingReplaceExistingCheck);
    setEnabled(m_testingCreateButton);
    setEnabled(m_testingAssignButton);
    setEnabled(m_testingAssignmentList);
    setEnabled(m_testingDeleteAssignmentButton);
    setEnabled(m_testingClassAddButton);
    setEnabled(m_testingClassDeleteButton);

    int previousClassId = -1;
    if (m_testingClassSelectedId > 0)
    {
        previousClassId = m_testingClassSelectedId;
    }
    else if (const auto item = m_testingClassSelector.SelectedItem().try_as<
            ComboBoxItem>())
    {
        previousClassId = boxedInt(item.Tag());
    }
    m_testingLoading = true;
    m_testingClassVisualLoading = true;
    m_testingClasses.clear();
    m_testingAssignments.clear();
    m_testingClassSelector.Items().Clear();
    m_testingClassList.Items().Clear();
    m_testingAssignmentList.Items().Clear();
    m_testingClassSelectedId = -1;
    if (m_testingValidationText)
    {
        m_testingValidationText.Text({});
        m_testingValidationText.Visibility(Visibility::Collapsed);
    }

    if (!hasDatabase)
    {
        m_testingStatusText.Text(L"No database open.");
        m_testingLoading = false;
        m_testingClassVisualLoading = false;
        loadTestingClassVisual(-1);
        m_testingClassVisualDirty = false;
        updateTestingClassActions();
        return;
    }

    classmngr::engine::TestingClassService classService(*m_openDatabase);
    classmngr::engine::TestingBlockService blockService(*m_openDatabase);
    const auto classes = classService.list();
    const auto assignments = blockService.listAssignments();
    if (!classes || !assignments)
    {
        const std::string message = !classes
            ? classes.error().message
            : assignments.error().message;
        m_testingStatusText.Text(winrt::hstring(
            L"Testing classes could not be loaded: " + asWide(message)
            ));
        if (m_testingValidationText)
        {
            m_testingValidationText.Text(winrt::hstring(
                L"Engine loading error: " + asWide(message)
                ));
            m_testingValidationText.Visibility(Visibility::Visible);
        }
        m_testingLoading = false;
        m_testingClassVisualLoading = false;
        updateTestingClassActions();
        return;
    }

    m_testingClasses = *classes;
    m_testingAssignments = *assignments;
    m_testingClassTeacherCombo.Items().Clear();
    {
        auto none = ComboBoxItem();
        none.Content(box_value(hstring(L"None")));
        none.Tag(box_value(-1));
        m_testingClassTeacherCombo.Items().Append(none);
    }
    classmngr::engine::TeacherService teacherService(*m_openDatabase);
    if (const auto teachers = teacherService.list())
    {
        for (const auto& teacher : *teachers)
        {
            if (teacher.teacherKr.empty())
            {
                continue;
            }
            auto item = ComboBoxItem();
            item.Content(box_value(hstring(asWide(teacher.teacherKr))));
            item.Tag(box_value(teacher.id));
            setAutomationName(
                item,
                L"Testing class teacher " + asWide(teacher.teacherKr)
                );
            m_testingClassTeacherCombo.Items().Append(item);
        }
    }

    int selectedClassIndex = -1;
    int selectedVisualIndex = -1;
    for (const auto& testingClass : m_testingClasses)
    {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(asWide(testingClass.name))));
        item.Tag(box_value(testingClass.classId));
        setAutomationName(item, L"Testing class " + asWide(testingClass.name));
        m_testingClassSelector.Items().Append(item);
        if (testingClass.classId == previousClassId)
        {
            selectedClassIndex = static_cast<int>(
                m_testingClassSelector.Items().Size() - 1
                );
        }

        auto listItem = ListViewItem();
        listItem.Content(box_value(hstring(testingClassLabel(testingClass))));
        listItem.Tag(box_value(testingClass.classId));
        listItem.HorizontalContentAlignment(HorizontalAlignment::Stretch);
        setAutomationName(
            listItem,
            L"Testing class list item " + asWide(testingClass.name)
            );
        m_testingClassList.Items().Append(listItem);
        if (testingClass.classId == previousClassId)
        {
            selectedVisualIndex = static_cast<int>(
                m_testingClassList.Items().Size() - 1
                );
        }
    }
    if (selectedClassIndex < 0 && m_testingClassSelector.Items().Size() > 0)
    {
        selectedClassIndex = 0;
    }
    if (selectedVisualIndex < 0 && m_testingClassList.Items().Size() > 0)
    {
        selectedVisualIndex = 0;
    }
    m_testingClassSelector.SelectedIndex(selectedClassIndex);
    m_testingClassList.SelectedIndex(selectedVisualIndex);

    const auto className = [this](int classId) {
        for (const auto& testingClass : m_testingClasses)
        {
            if (testingClass.classId == classId)
            {
                return asWide(testingClass.name);
            }
        }
        return std::wstring(L"Plain testing block");
    };
    for (const auto& assignment : m_testingAssignments)
    {
        auto item = ListViewItem();
        const std::wstring display = asWide(assignment.day) + L" "
            + asWide(assignment.startTime) + L" - "
            + className(assignment.classId)
            + (assignment.room.empty() ? L"" : L" (" + asWide(assignment.room) + L")");
        item.Content(box_value(hstring(display)));
        item.Tag(box_value(hstring(
            asWide(assignment.day) + L"|" + asWide(assignment.startTime)
            )));
        item.IsTabStop(false);
        setAutomationName(item, L"Testing assignment " + display);
        m_testingAssignmentList.Items().Append(item);
    }
    m_testingLoading = false;
    m_testingClassVisualLoading = false;
    const bool hasSelectedClass = m_testingClassSelector.SelectedIndex() >= 0;
    m_testingAssignButton.IsEnabled(hasDatabase && hasSelectedClass);
    m_testingDeleteAssignmentButton.IsEnabled(false);
    if (selectedVisualIndex >= 0)
    {
        const auto item = m_testingClassList.Items().GetAt(selectedVisualIndex)
            .try_as<ListViewItem>();
        loadTestingClassVisual(item ? boxedInt(item.Tag()) : -1);
    }
    else
    {
        loadTestingClassVisual(-1);
    }
    m_testingClassVisualDirty = false;
    m_testingStatusText.Text(winrt::hstring(
        L"Loaded " + std::to_wstring(m_testingClasses.size())
        + L" testing classes and "
        + std::to_wstring(m_testingAssignments.size())
        + L" assignments."
        ));
    updateTestingClassActions();
}

void MainWindow::loadTestingClassVisual(int classId)
{
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_testingClassNameTextBox || !m_testingClassRoomTextBox
        || !m_testingClassGradeCombo || !m_testingClassLevelCombo
        || !m_testingClassTeacherCombo || !m_testingClassNotesTextBox)
    {
        return;
    }

    m_testingClassVisualLoading = true;
    const classmngr::engine::TestingClass* selected = nullptr;
    for (const auto& testingClass : m_testingClasses)
    {
        if (testingClass.classId == classId)
        {
            selected = &testingClass;
            break;
        }
    }

    if (!selected)
    {
        m_testingClassSelectedId = -1;
        m_testingClassNameTextBox.Text(L"Testing Class");
        m_testingClassRoomTextBox.Text({});
        selectTestingComboValue(m_testingClassGradeCombo, L"M1");
        selectTestingComboValue(m_testingClassLevelCombo, L"Mixed (All)");
        m_testingClassTeacherCombo.SelectedIndex(0);
        m_testingClassNotesTextBox.Text({});
        m_testingClassColor = "#FFFFFF";
        m_testingClassFontColor = "#000000";
    }
    else
    {
        m_testingClassSelectedId = selected->classId;
        m_testingClassNameTextBox.Text(asWide(selected->name));
        m_testingClassRoomTextBox.Text(asWide(selected->room));
        selectTestingComboValue(
            m_testingClassGradeCombo,
            asWide(selected->grade)
            );
        selectTestingComboValue(
            m_testingClassLevelCombo,
            asWide(selected->level)
            );
        int teacherIndex = 0;
        for (int index = 0;
             index < static_cast<int>(m_testingClassTeacherCombo.Items().Size());
             ++index)
        {
            const auto item = m_testingClassTeacherCombo.Items().GetAt(index)
                .try_as<ComboBoxItem>();
            if (item && boxedInt(item.Tag()) == selected->teacherId)
            {
                teacherIndex = index;
                break;
            }
        }
        m_testingClassTeacherCombo.SelectedIndex(teacherIndex);
        m_testingClassNotesTextBox.Text(asWide(selected->notes));
        m_testingClassColor = selected->classColor.empty()
            ? "#FFFFFF"
            : selected->classColor;
        m_testingClassFontColor = selected->fontColor.empty()
            ? "#000000"
            : selected->fontColor;
    }

    m_testingClassGradeTextBox.Text(
        selectedComboValue(m_testingClassGradeCombo)
        );
    m_testingClassLevelTextBox.Text(
        selectedComboValue(m_testingClassLevelCombo)
        );
    setTestingPreviewColor(
        m_testingClassColorPreview,
        m_testingClassColor
        );
    setTestingPreviewColor(
        m_testingClassFontColorPreview,
        m_testingClassFontColor
        );

    if (m_testingClassSelector)
    {
        const bool wasLoading = m_testingLoading;
        m_testingLoading = true;
        int selectorIndex = -1;
        for (int index = 0;
             index < static_cast<int>(m_testingClassSelector.Items().Size());
             ++index)
        {
            const auto item = m_testingClassSelector.Items().GetAt(index)
                .try_as<ComboBoxItem>();
            if (item && boxedInt(item.Tag()) == m_testingClassSelectedId)
            {
                selectorIndex = index;
                break;
            }
        }
        m_testingClassSelector.SelectedIndex(selectorIndex);
        m_testingLoading = wasLoading;
    }

    m_testingClassVisualDirty = false;
    m_testingClassVisualLoading = false;
    updateTestingClassActions();
}

void MainWindow::updateTestingClassActions()
{
    const bool hasDatabase = static_cast<bool>(m_openDatabase);
    const auto setEnabled = [hasDatabase](auto const& control) {
        if (control)
        {
            control.IsEnabled(hasDatabase);
        }
    };
    setEnabled(m_testingClassList);
    setEnabled(m_testingClassNameTextBox);
    setEnabled(m_testingClassRoomTextBox);
    setEnabled(m_testingClassGradeCombo);
    setEnabled(m_testingClassLevelCombo);
    setEnabled(m_testingClassTeacherCombo);
    setEnabled(m_testingClassNotesTextBox);
    setEnabled(m_testingClassColorButton);
    setEnabled(m_testingClassFontColorButton);
    setEnabled(m_testingClassAddButton);
    if (m_testingClassDeleteButton)
    {
        m_testingClassDeleteButton.IsEnabled(
            hasDatabase && m_testingClassSelectedId > 0
            );
    }
    if (m_testingClassSaveButton)
    {
        m_testingClassSaveButton.Visibility(
            hasDatabase && m_testingClassVisualDirty
                ? Microsoft::UI::Xaml::Visibility::Visible
                : Microsoft::UI::Xaml::Visibility::Collapsed
            );
        m_testingClassSaveButton.IsEnabled(
            hasDatabase && m_testingClassVisualDirty
            );
    }
}

void MainWindow::beginTestingClass()
{
    if (!m_openDatabase)
    {
        return;
    }

    m_testingClassVisualLoading = true;
    m_testingClassSelectedId = -1;
    if (m_testingClassList)
    {
        m_testingClassList.SelectedIndex(-1);
    }
    if (m_testingClassSelector)
    {
        const bool wasLoading = m_testingLoading;
        m_testingLoading = true;
        m_testingClassSelector.SelectedIndex(-1);
        m_testingLoading = wasLoading;
    }
    m_testingClassNameTextBox.Text(L"Testing Class");
    m_testingClassRoomTextBox.Text({});
    selectTestingComboValue(m_testingClassGradeCombo, L"M1");
    selectTestingComboValue(m_testingClassLevelCombo, L"Mixed (All)");
    m_testingClassTeacherCombo.SelectedIndex(0);
    m_testingClassNotesTextBox.Text({});
    m_testingClassColor = "#FFFFFF";
    m_testingClassFontColor = "#000000";
    setTestingPreviewColor(m_testingClassColorPreview, m_testingClassColor);
    setTestingPreviewColor(
        m_testingClassFontColorPreview,
        m_testingClassFontColor
        );
    m_testingClassVisualLoading = false;
    m_testingClassVisualDirty = true;
    updateTestingClassActions();
}

void MainWindow::saveTestingClassDetails()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_openDatabase || !m_testingClassVisualDirty)
    {
        return;
    }

    m_testingClassGradeTextBox.Text(
        selectedComboValue(m_testingClassGradeCombo)
        );
    m_testingClassLevelTextBox.Text(
        selectedComboValue(m_testingClassLevelCombo)
        );
    if (m_testingClassSelectedId <= 0)
    {
        createTestingClass();
        return;
    }

    classmngr::engine::TestingClass testingClass;
    testingClass.classId = m_testingClassSelectedId;
    testingClass.name = asUtf8(m_testingClassNameTextBox.Text());
    testingClass.grade = asUtf8(selectedComboValue(m_testingClassGradeCombo));
    testingClass.level = asUtf8(selectedComboValue(m_testingClassLevelCombo));
    testingClass.room = asUtf8(m_testingClassRoomTextBox.Text());
    const auto teacherItem = m_testingClassTeacherCombo.SelectedItem().try_as<
        ComboBoxItem>();
    testingClass.teacherId = teacherItem ? boxedInt(teacherItem.Tag()) : -1;
    testingClass.classColor = m_testingClassColor;
    testingClass.fontColor = m_testingClassFontColor;
    testingClass.notes = asUtf8(m_testingClassNotesTextBox.Text());

    classmngr::engine::TestingClassService service(*m_openDatabase);
    const auto updated = service.update(testingClass);
    if (!updated)
    {
        m_testingStatusText.Text(L"Testing class could not be saved.");
        m_testingValidationText.Text(winrt::hstring(
            L"Engine validation error: " + asWide(updated.error().message)
            ));
        m_testingValidationText.Visibility(Visibility::Visible);
        return;
    }

    m_dirtyState.markDirty();
    updateFileCommandState();
    m_testingClassVisualDirty = false;
    refreshTestingWorkspace();
    loadTestingClassVisual(testingClass.classId);
}

void MainWindow::deleteTestingClass()
{
    using namespace Microsoft::UI::Xaml;

    if (!m_openDatabase || m_testingClassSelectedId <= 0)
    {
        return;
    }

    classmngr::engine::TestingClassService service(*m_openDatabase);
    const int deletedId = m_testingClassSelectedId;
    const auto deleted = service.remove(deletedId);
    if (!deleted)
    {
        m_testingStatusText.Text(L"Testing class could not be deleted.");
        m_testingValidationText.Text(winrt::hstring(
            L"Testing class deletion failed: " + asWide(deleted.error().message)
            ));
        m_testingValidationText.Visibility(Visibility::Visible);
        return;
    }

    m_dirtyState.markDirty();
    updateFileCommandState();
    m_testingClassSelectedId = -1;
    m_testingClassVisualDirty = false;
    refreshTestingWorkspace();
}

void MainWindow::createTestingClass()
{
    using namespace Microsoft::UI::Xaml;

    if (!m_openDatabase)
    {
        return;
    }
    if (m_testingClassGradeCombo && m_testingClassLevelCombo)
    {
        m_testingClassGradeTextBox.Text(
            selectedComboValue(m_testingClassGradeCombo)
            );
        m_testingClassLevelTextBox.Text(
            selectedComboValue(m_testingClassLevelCombo)
            );
    }
    classmngr::engine::TestingClass testingClass;
    testingClass.name = asUtf8(m_testingClassNameTextBox.Text());
    testingClass.grade = asUtf8(m_testingClassGradeTextBox.Text());
    testingClass.level = asUtf8(m_testingClassLevelTextBox.Text());
    testingClass.room = asUtf8(m_testingClassRoomTextBox.Text());
    if (m_testingClassTeacherCombo)
    {
        const auto teacherItem = m_testingClassTeacherCombo.SelectedItem()
            .try_as<Microsoft::UI::Xaml::Controls::ComboBoxItem>();
        testingClass.teacherId = teacherItem ? boxedInt(teacherItem.Tag()) : -1;
    }
    testingClass.classColor = m_testingClassColor;
    testingClass.fontColor = m_testingClassFontColor;
    if (m_testingClassNotesTextBox)
    {
        testingClass.notes = asUtf8(m_testingClassNotesTextBox.Text());
    }
    classmngr::engine::TestingClassService service(*m_openDatabase);
    const auto created = service.create(testingClass);
    if (!created)
    {
        m_testingStatusText.Text(L"Testing class could not be created.");
        m_testingValidationText.Text(winrt::hstring(
            L"Engine validation error: " + asWide(created.error().message)
            ));
        m_testingValidationText.Visibility(Visibility::Visible);
        return;
    }
    m_dirtyState.markDirty();
    updateFileCommandState();
    m_testingClassVisualDirty = false;
    refreshTestingWorkspace();
    for (int index = 0;
         index < static_cast<int>(m_testingClassSelector.Items().Size());
         ++index)
    {
        const auto item = m_testingClassSelector.Items().GetAt(index)
            .try_as<Microsoft::UI::Xaml::Controls::ComboBoxItem>();
        if (item && boxedInt(item.Tag()) == *created)
        {
            m_testingClassSelector.SelectedIndex(index);
            break;
        }
    }
    loadTestingClassVisual(*created);
    m_testingStatusText.Text(L"Testing class created.");
}

void MainWindow::assignTestingClass()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_openDatabase || !m_testingClassSelector || !m_testingDayCombo
        || !m_testingStartTextBox)
    {
        return;
    }
    const auto classItem = m_testingClassSelector.SelectedItem().try_as<
        ComboBoxItem>();
    const int classId = classItem ? boxedInt(classItem.Tag()) : -1;
    const std::wstring day = selectedComboValue(m_testingDayCombo);
    const std::wstring start = m_testingStartTextBox.Text().c_str();
    if (classId <= 0 || day.empty() || start.empty())
    {
        m_testingStatusText.Text(L"Testing assignment could not be saved.");
        m_testingValidationText.Text(
            L"Choose a testing class, weekday, and strict HH:mm start time."
            );
        m_testingValidationText.Visibility(Visibility::Visible);
        return;
    }
    const auto checked = m_testingReplaceExistingCheck.IsChecked();
    const bool replaceExisting = checked && checked.Value();
    classmngr::engine::TestingBlockService service(*m_openDatabase);
    const auto saved = service.assignClass(
        asUtf8(day),
        asUtf8(start),
        classId,
        replaceExisting
        );
    if (!saved)
    {
        m_testingStatusText.Text(L"Testing assignment could not be saved.");
        m_testingValidationText.Text(winrt::hstring(
            L"Assignment validation error: " + asWide(saved.error().message)
            ));
        m_testingValidationText.Visibility(Visibility::Visible);
        return;
    }
    m_dirtyState.markDirty();
    updateFileCommandState();
    refreshTestingWorkspace();
    refreshScheduleBoard();
    m_testingStatusText.Text(L"Testing assignment saved.");
}

void MainWindow::deleteTestingAssignment()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_openDatabase || !m_testingAssignmentList)
    {
        return;
    }
    const auto item = m_testingAssignmentList.SelectedItem().try_as<
        ListViewItem>();
    if (!item)
    {
        return;
    }
    const auto parts = splitScheduleKey(boxedString(item.Tag()));
    if (parts.size() != 2 || parts[0].empty() || parts[1].empty())
    {
        return;
    }
    classmngr::engine::TestingBlockService service(*m_openDatabase);
    const auto deleted = service.deleteAssignment(
        asUtf8(parts[0]),
        asUtf8(parts[1])
        );
    if (!deleted)
    {
        m_testingStatusText.Text(L"Testing assignment could not be deleted.");
        m_testingValidationText.Text(winrt::hstring(
            L"Assignment deletion failed: " + asWide(deleted.error().message)
            ));
        m_testingValidationText.Visibility(Visibility::Visible);
        return;
    }
    m_dirtyState.markDirty();
    updateFileCommandState();
    refreshTestingWorkspace();
    refreshScheduleBoard();
    m_testingStatusText.Text(L"Testing assignment deleted.");
}

} // namespace winrt::ClassMngrWinUI::implementation
