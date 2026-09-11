#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::refreshTestingWorkspace()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_testingClassSelector || !m_testingAssignmentList
        || !m_testingStatusText)
    {
        return;
    }

    const auto checked = [](auto const& check) {
        const auto value = check.IsChecked();
        return value && value.Value();
    };
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
    setEnabled(m_testingDayCombo);
    setEnabled(m_testingStartTextBox);
    setEnabled(m_testingReplaceExistingCheck);
    setEnabled(m_testingCreateButton);
    setEnabled(m_testingAssignButton);
    setEnabled(m_testingAssignmentList);
    setEnabled(m_testingDeleteAssignmentButton);

    int previousClassId = -1;
    if (const auto item = m_testingClassSelector.SelectedItem().try_as<
            ComboBoxItem>())
    {
        previousClassId = boxedInt(item.Tag());
    }
    m_testingLoading = true;
    m_testingClasses.clear();
    m_testingAssignments.clear();
    m_testingClassSelector.Items().Clear();
    m_testingAssignmentList.Items().Clear();
    if (m_testingValidationText)
    {
        m_testingValidationText.Text({});
        m_testingValidationText.Visibility(Visibility::Collapsed);
    }

    if (!hasDatabase)
    {
        m_testingStatusText.Text(L"No database open.");
        m_testingLoading = false;
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
        return;
    }

    m_testingClasses = *classes;
    m_testingAssignments = *assignments;
    int selectedClassIndex = -1;
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
    }
    if (selectedClassIndex < 0 && m_testingClassSelector.Items().Size() > 0)
    {
        selectedClassIndex = 0;
    }
    m_testingClassSelector.SelectedIndex(selectedClassIndex);

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
    const bool hasSelectedClass = m_testingClassSelector.SelectedIndex() >= 0;
    m_testingAssignButton.IsEnabled(hasDatabase && hasSelectedClass);
    m_testingDeleteAssignmentButton.IsEnabled(false);
    m_testingStatusText.Text(winrt::hstring(
        L"Loaded " + std::to_wstring(m_testingClasses.size())
        + L" testing classes and "
        + std::to_wstring(m_testingAssignments.size())
        + L" assignments."
        ));
}

void MainWindow::createTestingClass()
{
    using namespace Microsoft::UI::Xaml;

    if (!m_openDatabase)
    {
        return;
    }
    classmngr::engine::TestingClass testingClass;
    testingClass.name = asUtf8(m_testingClassNameTextBox.Text());
    testingClass.grade = asUtf8(m_testingClassGradeTextBox.Text());
    testingClass.level = asUtf8(m_testingClassLevelTextBox.Text());
    testingClass.room = asUtf8(m_testingClassRoomTextBox.Text());
    testingClass.classColor = "#FFFFFF";
    testingClass.fontColor = "#000000";
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
    refreshTestingWorkspace();
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
    refreshTestingWorkspace();
    m_testingStatusText.Text(L"Testing assignment deleted.");
}

} // namespace winrt::ClassMngrWinUI::implementation
