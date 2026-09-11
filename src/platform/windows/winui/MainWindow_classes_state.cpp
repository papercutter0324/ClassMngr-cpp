#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::updateClassActions()
{
    if (!m_classSelector || !m_classNameTextBox || !m_classSaveButton
        || !m_classNotesSaveButton
        || !m_classCoTeacherKrCombo
        || !m_classCoTeacherSaveButton)
    {
        return;
    }

    const bool hasDatabase = static_cast<bool>(m_openDatabase);
    const bool hasClass = m_classSelectedId > 0 || m_classNew;
    const bool clean = !m_classDirty && !m_classRosterDirty
        && !m_speakingEvaluationDirty;
    const bool detailsEnabled = hasDatabase && hasClass;
    const bool notesEnabled = hasDatabase && m_classSelectedId > 0;

    m_classSelector.IsEnabled(hasDatabase && clean && !m_classNew);
    m_classNameTextBox.IsEnabled(detailsEnabled);
    m_classGradeCombo.IsEnabled(detailsEnabled);
    m_classLevelCombo.IsEnabled(detailsEnabled);
    m_classReadingBookCombo.IsEnabled(detailsEnabled);
    m_classEssayBookCombo.IsEnabled(detailsEnabled);
    m_classColorTextBox.IsEnabled(detailsEnabled);
    m_classColorPreview.IsHitTestVisible(detailsEnabled);
    m_classColorChooseButton.IsEnabled(detailsEnabled);
    m_classFontColorTextBox.IsEnabled(detailsEnabled);
    for (const auto& control : m_classRegularDayCombos)
    {
        control.IsEnabled(detailsEnabled);
    }
    for (const auto& control : m_classRegularStartHourCombos)
    {
        control.IsEnabled(detailsEnabled);
    }
    for (const auto& control : m_classRegularStartMinuteCombos)
    {
        control.IsEnabled(detailsEnabled);
    }
    for (const auto& control : m_classRegularStartPeriodCombos)
    {
        control.IsEnabled(detailsEnabled);
    }
    for (const auto& control : m_classRegularEndCombos)
    {
        control.IsEnabled(detailsEnabled);
    }
    for (const auto& control : m_classIntensiveDayCombos)
    {
        control.IsEnabled(detailsEnabled);
    }
    for (const auto& control : m_classIntensiveStartHourCombos)
    {
        control.IsEnabled(detailsEnabled);
    }
    for (const auto& control : m_classIntensiveStartMinuteCombos)
    {
        control.IsEnabled(detailsEnabled);
    }
    for (const auto& control : m_classIntensiveStartPeriodCombos)
    {
        control.IsEnabled(detailsEnabled);
    }
    for (const auto& control : m_classIntensiveEndCombos)
    {
        control.IsEnabled(detailsEnabled);
    }
    m_classRegularScheduleAddButton.IsEnabled(detailsEnabled);
    m_classIntensiveScheduleAddButton.IsEnabled(detailsEnabled);
    m_classNotesTextBox.IsEnabled(notesEnabled || (hasDatabase && m_classNew));
    m_classTimeFillerActivitiesTextBox.IsEnabled(
        notesEnabled || (hasDatabase && m_classNew)
        );
    m_classCoTeacherKrCombo.IsEnabled(detailsEnabled);
    m_classCoTeacherEnCombo.IsEnabled(detailsEnabled);
    m_classCoTeacherRoomTextBox.IsEnabled(detailsEnabled);
    m_classCoTeacherInternetTypeTextBox.IsEnabled(detailsEnabled);
    m_classCoTeacherWifiNameTextBox.IsEnabled(detailsEnabled);
    m_classCoTeacherWifiPasswordTextBox.IsEnabled(detailsEnabled);
    m_classCoTeacherProjectionTypeTextBox.IsEnabled(detailsEnabled);
    m_classCoTeacherZoomIdTextBox.IsEnabled(detailsEnabled);
    m_classCoTeacherZoomPasswordTextBox.IsEnabled(detailsEnabled);

    m_classNewButton.IsEnabled(hasDatabase && clean && !m_classNew);
    m_classDeleteButton.IsEnabled(
        hasDatabase && clean && !m_classNew && m_classSelectedId > 0
        );
    m_classSaveButton.IsEnabled(
        hasDatabase && hasClass && m_classDetailsDirty
        );
    m_classDiscardButton.IsEnabled(
        hasDatabase && hasClass && m_classDirty
        );
    m_classNotesSaveButton.IsEnabled(
        hasDatabase && m_classSelectedId > 0 && m_classNotesDirty
        );
    m_classNotesDiscardButton.IsEnabled(
        hasDatabase && m_classSelectedId > 0 && m_classNotesDirty
        );
    m_classCoTeacherSaveButton.IsEnabled(
        hasDatabase && m_classSelectedId > 0 && m_classDetailsDirty
        );
    m_classCoTeacherDiscardButton.IsEnabled(
        hasDatabase && m_classSelectedId > 0 && m_classDetailsDirty
        );
    updateClassRosterActions();
}

void MainWindow::markClassDirty()
{
    if (m_classLoading || !m_openDatabase)
    {
        return;
    }

    m_classDirty = true;
    m_dirtyState.markDirty();
    if (m_classStatusText)
    {
        m_classStatusText.Text(L"Unsaved class information changes.");
    }
    if (m_classNotesStatusText)
    {
        m_classNotesStatusText.Text(L"Unsaved class notes changes.");
    }
    updateClassActions();
}

void MainWindow::clearClassDirty()
{
    m_classDirty = false;
    m_classDetailsDirty = false;
    m_classNotesDirty = false;
    if (!m_classRosterDirty && !m_speakingEvaluationDirty)
    {
        m_dirtyState.markClean();
    }
    updateClassActions();
}

void MainWindow::presentClass(int index)
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_classSelector || !m_classNameTextBox)
    {
        return;
    }

    m_classLoading = true;
    classmngr::engine::Classroom classroom;
    if (index >= 0 && index < static_cast<int>(m_classes.size()))
    {
        classroom = m_classes[static_cast<std::size_t>(index)];
        m_classSelectedIndex = index;
        m_classSelectedId = classroom.id;
    }
    else
    {
        m_classSelectedIndex = -1;
        m_classSelectedId = -1;
    }

    classmngr::engine::ClassInfo info;
    bool loaded = false;
    if (classroom.id > 0 && m_openDatabase)
    {
        classmngr::engine::ClassInfoService service(*m_openDatabase);
        const auto result = service.load(classroom.id);
        if (result)
        {
            info = *result;
            loaded = true;
        }
        else if (m_classValidationText)
        {
            m_classValidationText.Text(winrt::hstring(
                L"Class information could not be loaded: "
                + asWide(result.error().message)
                ));
            m_classValidationText.Visibility(
                Microsoft::UI::Xaml::Visibility::Visible
                );
        }
    }
    info.classId = classroom.id;
    m_classInfo = info;
    refreshClassCoTeacher();

    m_classNameTextBox.Text(asWide(classroom.name));
    m_classGradeCombo.SelectedIndex(0);
    refreshClassInformationOptions();

    const auto selectChoice = [](ComboBox combo, std::wstring_view value) {
        for (int optionIndex = 0;
             optionIndex < static_cast<int>(combo.Items().Size());
             ++optionIndex)
        {
            const auto item = combo.Items().GetAt(optionIndex).try_as<ComboBoxItem>();
            if (item && boxedString(item.Tag()) == value)
            {
                combo.SelectedIndex(optionIndex);
                return;
            }
        }
        combo.SelectedIndex(-1);
    };
    selectChoice(m_classGradeCombo, asWide(info.classGrade));
    refreshClassInformationOptions();
    selectChoice(m_classLevelCombo, asWide(info.classLevel));
    refreshClassInformationOptions();
    selectChoice(m_classReadingBookCombo, asWide(info.readingBook));
    selectChoice(m_classEssayBookCombo, asWide(info.essayBook));
    m_classColorTextBox.Text(
        asWide(info.classColor.empty() ? "#FFFFFF" : info.classColor)
        );
    if (m_classColorPreview)
    {
        m_classColorPreview.Background(
            Microsoft::UI::Xaml::Media::SolidColorBrush(
                uiColorFromHex(
                    info.classColor.empty() ? "#FFFFFF" : info.classColor
                    )
                )
            );
    }
    m_classFontColorTextBox.Text(
        asWide(info.fontColor.empty() ? "#000000" : info.fontColor)
        );

    if (m_classStudentCountTextBox)
    {
        std::wstring count = L"0";
        if (classroom.id > 0 && m_openDatabase)
        {
            classmngr::engine::RosterService rosterService(*m_openDatabase);
            const auto studentCount = rosterService.studentCount(classroom.id);
            if (studentCount)
            {
                count = std::to_wstring(*studentCount);
            }
        }
        m_classStudentCountTextBox.Text(winrt::hstring(count));
    }
    rebuildClassScheduleRows(false, info.classTimes);
    rebuildClassScheduleRows(true, info.intensiveTimes);

    std::wstring teacher = asWide(info.teacherPreferredName);
    if (teacher.empty())
    {
        teacher = asWide(info.teacherEn);
    }
    if (teacher.empty())
    {
        teacher = asWide(info.teacherKr);
    }
    m_classTeacherText.Text(
        winrt::hstring(
            teacher.empty()
                ? L"Assigned teacher: Unassigned"
                : L"Assigned teacher: " + teacher
            )
        );

    m_classNotesTextBox.Text(asWide(info.notes));
    m_classTimeFillerActivitiesTextBox.Text(
        asWide(info.timeFillerActivities)
        );
    m_classLoading = false;

    if (m_classValidationText && loaded)
    {
        m_classValidationText.Text({});
        m_classValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Collapsed
            );
    }
    updateClassActions();
}

} // namespace winrt::ClassMngrWinUI::implementation
