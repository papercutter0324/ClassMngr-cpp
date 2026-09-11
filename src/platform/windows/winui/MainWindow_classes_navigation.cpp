#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::refreshClassNavigation(bool selectFallback)
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_classNavigationGradeTabs
        || !m_classNavigationDayTabs
        || !m_classNavigationClassTabs)
    {
        return;
    }

    m_classNavigationLoading = true;
    m_classNavigationGradeTabs.Children().Clear();
    m_classNavigationDayTabs.Children().Clear();
    m_classNavigationClassTabs.Children().Clear();

    if (!m_openDatabase || m_classes.empty())
    {
        m_classNavigationGrade.clear();
        m_classNavigationSelectedDays.clear();
        m_classNavigationAll = true;
        auto empty = TextBlock();
        empty.Text(
            !m_openDatabase
                ? L"Open a database to browse scheduled classes."
                : L"No classes are available."
            );
        empty.TextWrapping(TextWrapping::Wrap);
        setAutomationName(empty, L"Class navigation empty state");
        m_classNavigationClassTabs.Children().Append(empty);
        m_classNavigationLoading = false;
        return;
    }

    classmngr::engine::ClassInfoService infoService(*m_openDatabase);
    std::vector<classmngr::engine::ClassTabNavigationService::ClassEntry>
        entries;
    entries.reserve(m_classes.size());
    for (const classmngr::engine::Classroom& classroom : m_classes)
    {
        if (classroom.id <= 0)
        {
            continue;
        }

        classmngr::engine::ClassInfo info;
        const auto loaded = infoService.load(classroom.id);
        if (loaded)
        {
            info = *loaded;
        }

        classmngr::engine::ClassTabNavigationService::ClassEntry entry;
        entry.classId = classroom.id;
        entry.classroomName = classroom.name;
        entry.grade = info.classGrade;
        entry.level = info.classLevel;
        entry.regularTimes = info.classTimes;
        entry.intensiveTimes = info.intensiveTimes;
        entry.teacherEn = info.teacherEn;
        entry.teacherKr = info.teacherKr;
        entries.push_back(std::move(entry));
    }

    const auto isWeekendDay = [](std::string value) {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](char character) {
                return static_cast<char>(
                    std::tolower(static_cast<unsigned char>(character))
                    );
            }
            );
        return value == "saturday" || value == "sunday";
    };
    const auto includesWeekend = [&isWeekendDay](
        const std::vector<classmngr::engine::ClassTime>& times
        ) {
        return std::any_of(
            times.begin(),
            times.end(),
            [&isWeekendDay](const auto& time) {
                return isWeekendDay(time.day);
            }
            );
    };
    const bool weekendAvailable = std::any_of(
        entries.begin(),
        entries.end(),
        [&includesWeekend](const auto& entry) {
            return includesWeekend(entry.regularTimes)
                || includesWeekend(entry.intensiveTimes);
        }
        );

    if (!weekendAvailable)
    {
        m_classNavigationSelectedDays.erase(
            std::remove(
                m_classNavigationSelectedDays.begin(),
                m_classNavigationSelectedDays.end(),
                "Wkend"
                ),
            m_classNavigationSelectedDays.end()
            );
    }

    classmngr::engine::ClassTabNavigationService::DayFilter dayFilter;
    dayFilter.selectedDays = m_classNavigationSelectedDays;
    dayFilter.scheduleSource =
        classmngr::engine::ClassTabNavigationService::ScheduleSource::Regular;
    dayFilter.visibilityScope =
        classmngr::engine::ClassTabNavigationService::VisibilityScope::ActiveSchedule;
    const auto navigation =
        classmngr::engine::ClassTabNavigationService::build(
            entries,
            classmngr::engine::ClassTabNavigationService::GroupingPolicy::AlwaysGradeGrouped,
            dayFilter
            );

    if (!m_classNavigationAll)
    {
        const bool gradeAvailable = std::any_of(
            navigation.gradeGroups.begin(),
            navigation.gradeGroups.end(),
            [this](const auto& group) {
                return group.grade == m_classNavigationGrade;
            }
            );
        if (!gradeAvailable)
        {
            m_classNavigationAll = true;
            m_classNavigationGrade.clear();
        }
    }

    const auto setButtonState = [](Button const& button, bool selected) {
        button.Background(
            Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::Color{
                    255,
                    static_cast<std::uint8_t>(selected ? 59 : 48),
                    static_cast<std::uint8_t>(selected ? 169 : 53),
                    static_cast<std::uint8_t>(selected ? 225 : 60)
                }
                )
            );
        button.Foreground(
            Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::Color{255, 255, 255, 255}
                )
            );
        button.BorderBrush(
            Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::Color{255, 92, 99, 108}
                )
            );
        button.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
        button.CornerRadius(CornerRadius{5.0, 5.0, 5.0, 5.0});
    };

    const auto makeNavigationButton = [](std::wstring const& label) {
        auto button = Button();
        button.Content(box_value(hstring(label)));
        button.MinHeight(34.0);
        button.Padding(Thickness{12.0, 4.0, 12.0, 4.0});
        button.IsTabStop(true);
        button.HorizontalAlignment(HorizontalAlignment::Left);
        return button;
    };

    for (const auto& group : navigation.gradeGroups)
    {
        const std::wstring label = asWide(group.label);
        auto button = makeNavigationButton(label);
        setAutomationName(
            button,
            std::wstring(L"Class grade filter ") + label
            );
        setButtonState(
            button,
            !m_classNavigationAll
                && group.grade == m_classNavigationGrade
            );
        const std::string grade = group.grade;
        button.Click(
            [this, grade](auto const&, auto const&) {
                selectClassNavigationGrade(false, grade);
            }
            );
        m_classNavigationGradeTabs.Children().Append(button);
    }

    if (!navigation.allClasses.empty())
    {
        auto button = makeNavigationButton(L"All");
        setAutomationName(button, L"Class grade filter All");
        setButtonState(button, m_classNavigationAll);
        button.Click(
            [this](auto const&, auto const&) {
                selectClassNavigationGrade(true, {});
            }
            );
        m_classNavigationGradeTabs.Children().Append(button);
    }

    const std::array<std::pair<std::wstring_view, std::string_view>, 6>
        dayButtons{
            {
                {L"M", "Monday"},
                {L"T", "Tuesday"},
                {L"W", "Wednesday"},
                {L"Th", "Thursday"},
                {L"F", "Friday"},
                {L"Wkend", "Wkend"}
            }
        };
    for (const auto& definition : dayButtons)
    {
        if (definition.second == "Wkend" && !weekendAvailable)
        {
            continue;
        }

        const std::wstring label(definition.first);
        auto button = makeNavigationButton(label);
        setAutomationName(
            button,
            std::wstring(L"Class day filter ") + asWide(definition.second)
            );
        const std::string day(definition.second);
        const bool selected = std::find(
            m_classNavigationSelectedDays.begin(),
            m_classNavigationSelectedDays.end(),
            day
            ) != m_classNavigationSelectedDays.end();
        setButtonState(button, selected);
        button.Click(
            [this, day](auto const&, auto const&) {
                toggleClassNavigationDay(day);
            }
            );
        m_classNavigationDayTabs.Children().Append(button);
    }

    const std::vector<
        classmngr::engine::ClassTabNavigationService::ClassTab>* visibleClasses =
        &navigation.allClasses;
    if (!m_classNavigationAll)
    {
        visibleClasses = nullptr;
        for (const auto& group : navigation.gradeGroups)
        {
            if (group.grade == m_classNavigationGrade)
            {
                visibleClasses = &group.classes;
                break;
            }
        }
    }

    if (!visibleClasses || visibleClasses->empty())
    {
        auto empty = TextBlock();
        empty.Text(L"No scheduled classes match the current filters.");
        empty.TextWrapping(TextWrapping::Wrap);
        setAutomationName(
            empty,
            L"No scheduled classes match the current filters"
            );
        m_classNavigationClassTabs.Children().Append(empty);
    }
    else
    {
        for (const auto& classTab : *visibleClasses)
        {
            std::wstring label = asWide(classTab.label);
            if (label.empty())
            {
                label = L"Class " + std::to_wstring(classTab.classId);
            }
            auto button = makeNavigationButton(label);
            setAutomationName(
                button,
                std::wstring(L"Class tab ") + label
                );
            setButtonState(button, classTab.classId == m_classSelectedId);
            const int classId = classTab.classId;
            button.Click(
                [this, classId](auto const&, auto const&) {
                    selectClassFromNavigation(classId);
                }
                );
            m_classNavigationClassTabs.Children().Append(button);
        }
    }

    int fallbackClassId = -1;
    if (visibleClasses && !visibleClasses->empty())
    {
        const auto current = std::find_if(
            visibleClasses->begin(),
            visibleClasses->end(),
            [this](const auto& classTab) {
                return classTab.classId == m_classSelectedId;
            }
            );
        if (current == visibleClasses->end())
        {
            fallbackClassId = visibleClasses->front().classId;
        }
    }

    m_classNavigationLoading = false;
    const bool clean = !m_classDirty
        && !m_classRosterDirty
        && !m_speakingEvaluationDirty
        && !m_classNew;
    if (selectFallback
        && clean
        && fallbackClassId > 0
        && fallbackClassId != m_classSelectedId)
    {
        selectClassFromNavigation(fallbackClassId);
    }
}

void MainWindow::selectClassFromNavigation(int classId)
{
    if (!m_openDatabase || !m_classSelector || m_classNew)
    {
        return;
    }

    if (m_classRosterDirty && !m_classDirty && !m_speakingEvaluationDirty)
    {
        confirmClassRosterNavigation([this, classId]() {
            selectClassFromNavigation(classId);
        });
        return;
    }

    if (m_classDirty || m_classRosterDirty || m_speakingEvaluationDirty)
    {
        m_classStatusText.Text(
            m_speakingEvaluationDirty
                ? L"Save or discard the current speaking evaluation before selecting another."
                : m_classRosterDirty
                    ? L"Save or discard the current roster before selecting another."
                    : L"Save or discard the current class before selecting another."
            );
        return;
    }

    int resolvedIndex = -1;
    for (int index = 0; index < static_cast<int>(m_classes.size()); ++index)
    {
        if (m_classes[static_cast<std::size_t>(index)].id == classId)
        {
            resolvedIndex = index;
            break;
        }
    }

    if (resolvedIndex == m_classSelectedIndex)
    {
        refreshClassNavigation(false);
        return;
    }

    m_classSelector.SelectedIndex(resolvedIndex);
}

void MainWindow::selectClassNavigationGrade(bool all, std::string grade)
{
    if (m_classNavigationLoading)
    {
        return;
    }
    if (m_classRosterDirty && !m_classDirty && !m_speakingEvaluationDirty)
    {
        confirmClassRosterNavigation([this, all, grade = std::move(grade)]() mutable {
            selectClassNavigationGrade(all, std::move(grade));
        });
        return;
    }
    if (m_classDirty || m_classRosterDirty || m_speakingEvaluationDirty)
    {
        m_classStatusText.Text(
            m_speakingEvaluationDirty
                ? L"Save or discard the current speaking evaluation before changing class filters."
                : m_classRosterDirty
                    ? L"Save or discard the current roster before changing class filters."
                    : L"Save or discard the current class before changing class filters."
            );
        return;
    }

    m_classNavigationAll = all;
    m_classNavigationGrade = all ? std::string{} : std::move(grade);
    refreshClassNavigation(true);
}

void MainWindow::toggleClassNavigationDay(std::string day)
{
    if (m_classNavigationLoading)
    {
        return;
    }
    if (m_classRosterDirty && !m_classDirty && !m_speakingEvaluationDirty)
    {
        confirmClassRosterNavigation([this, day = std::move(day)]() mutable {
            toggleClassNavigationDay(std::move(day));
        });
        return;
    }
    if (m_classDirty || m_classRosterDirty || m_speakingEvaluationDirty)
    {
        m_classStatusText.Text(
            m_speakingEvaluationDirty
                ? L"Save or discard the current speaking evaluation before changing class filters."
                : m_classRosterDirty
                    ? L"Save or discard the current roster before changing class filters."
                    : L"Save or discard the current class before changing class filters."
            );
        return;
    }

    const auto found = std::find(
        m_classNavigationSelectedDays.begin(),
        m_classNavigationSelectedDays.end(),
        day
        );
    if (found == m_classNavigationSelectedDays.end())
    {
        m_classNavigationSelectedDays.push_back(std::move(day));
    }
    else
    {
        m_classNavigationSelectedDays.erase(found);
    }
    refreshClassNavigation(true);
}

void MainWindow::refreshClassNavigationLocation()
{
    m_classNavigationLocation = ClassNavigationLocation::Top;
    if (m_openDatabase)
    {
        classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
        const auto loaded = settings.load(classNavigationLocationKey);
        if (loaded)
        {
            const auto stored = classNavigationLocationFromSetting(*loaded);
            if (stored)
            {
                m_classNavigationLocation = *stored;
            }
        }
    }
    applyClassNavigationLayout();
}

void MainWindow::applyClassNavigationLayout()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_classPageRoot
        || !m_classSectionContentHost
        || !m_classSectionTitle
        || !m_classSectionActionsHost
        || !m_classNavigationCard)
    {
        return;
    }

    const bool bottom = m_classNavigationLocation
        == ClassNavigationLocation::Bottom
        && static_cast<bool>(m_openDatabase);
    // The title stays immediately below the navigation card, while the action
    // footer remains the final row so it stays at the viewport bottom in
    // either navigation mode.
    m_classPageRoot.RowDefinitions().GetAt(1).Height(
        GridLengthHelper::FromValueAndType(
            1.0,
            bottom ? GridUnitType::Star : GridUnitType::Auto
            )
        );
    m_classPageRoot.RowDefinitions().GetAt(2).Height(
        GridLengthHelper::FromValueAndType(1.0, GridUnitType::Auto)
        );
    m_classPageRoot.RowDefinitions().GetAt(3).Height(
        GridLengthHelper::FromValueAndType(
            1.0,
            bottom ? GridUnitType::Auto : GridUnitType::Star
            )
        );
    m_classPageRoot.RowDefinitions().GetAt(4).Height(
        GridLengthHelper::FromValueAndType(1.0, GridUnitType::Auto)
        );
    // With bottom navigation the content occupies row 1, navigation row 2,
    // title row 3, and footer row 4. With top navigation the navigation card
    // is row 1, title row 2, content row 3, and footer row 4.
    if (bottom)
    {
        Grid::SetRow(m_classSectionContentHost, 1);
        Grid::SetRow(m_classNavigationCard, 2);
        Grid::SetRow(m_classSectionTitle, 3);
        Grid::SetRow(m_classSectionActionsHost, 4);
    }
    else
    {
        Grid::SetRow(m_classNavigationCard, 1);
        Grid::SetRow(m_classSectionTitle, 2);
        Grid::SetRow(m_classSectionContentHost, 3);
        Grid::SetRow(m_classSectionActionsHost, 4);
    }
}

void MainWindow::selectClassSection(int index)
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (index < 0
        || index >= static_cast<int>(m_classSectionScrollViews.size())
        || !m_classSectionContentHost)
    {
        return;
    }

    m_classSectionIndex = index;
    m_classSectionContentHost.Content(
        m_classSectionScrollViews[static_cast<std::size_t>(index)]
        );

    if (m_classSectionTitle)
    {
        m_classSectionTitle.Text(winrt::hstring(
            std::wstring(classSectionTitles[static_cast<std::size_t>(index)])
            ));
    }

    if (m_classSectionActionsHost)
    {
        if (index == 1 && m_classRosterActions)
        {
            m_classSectionActionsHost.Content(m_classRosterActions);
            m_classSectionActionsHost.Visibility(Visibility::Visible);
        }
        else if (index == 3 && m_speakingEvaluationActions)
        {
            m_classSectionActionsHost.Content(m_speakingEvaluationActions);
            m_classSectionActionsHost.Visibility(Visibility::Visible);
        }
        else
        {
            m_classSectionActionsHost.Visibility(Visibility::Collapsed);
        }
    }

    if (m_classSectionSelectorBar
        && m_classSectionSelectorItems[static_cast<std::size_t>(index)])
    {
        m_classSectionSelectionChanging = true;
        m_classSectionSelectorBar.SelectedItem(
            m_classSectionSelectorItems[static_cast<std::size_t>(index)]
            );
        m_classSectionSelectionChanging = false;
    }
    updateFileCommandState();
}

void MainWindow::refreshClassesPage()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_classSelector || !m_classStatusText)
    {
        return;
    }

    refreshClassNavigationLocation();

    if (!m_openDatabase)
    {
        m_classLoading = true;
        m_classes.clear();
        m_classSelectedIndex = -1;
        m_classSelectedId = -1;
        m_classNew = false;
        m_classSelector.Items().Clear();
        presentClass(-1);
        refreshClassNavigation(false);
        m_classLoading = false;
        m_classStatusText.Text(L"No database open.");
        m_classNotesStatusText.Text(L"No database open.");
        m_classValidationText.Text({});
        m_classValidationText.Visibility(Visibility::Collapsed);
        m_classNotesValidationText.Text({});
        m_classNotesValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Collapsed
            );
        clearClassDirty();
        refreshClassRoster();
        refreshSpeakingEvaluation();
        refreshSpeakingAnalytics();
        return;
    }

    classmngr::engine::ClassRepository repository(*m_openDatabase);
    const auto loaded = repository.list();
    if (!loaded)
    {
        m_classLoading = true;
        m_classes.clear();
        m_classSelector.Items().Clear();
        presentClass(-1);
        refreshClassNavigation(false);
        m_classLoading = false;
        m_classStatusText.Text(winrt::hstring(
            L"Classes could not be loaded: " + asWide(loaded.error().message)
            ));
        m_classNotesStatusText.Text(L"Class notes are unavailable.");
        m_classValidationText.Text(winrt::hstring(
            L"Engine loading error: " + asWide(loaded.error().message)
            ));
        m_classValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_classNotesValidationText.Text(winrt::hstring(
            L"Engine loading error: " + asWide(loaded.error().message)
            ));
        m_classNotesValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        clearClassDirty();
        refreshClassRoster();
        refreshSpeakingEvaluation();
        refreshSpeakingAnalytics();
        return;
    }

    const int previousId = m_classSelectedId;
    m_classes = *loaded;
    m_classLoading = true;
    m_classSelector.Items().Clear();
    int selectedIndex = -1;
    for (int index = 0; index < static_cast<int>(m_classes.size()); ++index)
    {
        const auto& classroom = m_classes[static_cast<std::size_t>(index)];
        std::wstring display = asWide(classroom.name);
        if (display.empty())
        {
            display = L"Class " + std::to_wstring(classroom.id);
        }
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(display)));
        item.Tag(box_value(classroom.id));
        setAutomationName(item, L"Class " + display);
        m_classSelector.Items().Append(item);
        if (classroom.id == previousId)
        {
            selectedIndex = index;
        }
    }
    if (selectedIndex < 0 && !m_classes.empty())
    {
        selectedIndex = 0;
    }
    m_classSelectedIndex = selectedIndex;
    m_classSelectedId = selectedIndex >= 0
        ? m_classes[static_cast<std::size_t>(selectedIndex)].id
        : -1;
    m_classSelector.SelectedIndex(selectedIndex);
    m_classLoading = false;

    if (selectedIndex >= 0)
    {
        presentClass(selectedIndex);
        m_classStatusText.Text({});
        m_classNotesStatusText.Text(L"Select a class tab to edit notes.");
    }
    else
    {
        presentClass(-1);
        m_classStatusText.Text(
            L"No classes found. Choose New Class to add one."
            );
        m_classNotesStatusText.Text(L"No class selected.");
    }
    m_classValidationText.Text({});
    m_classValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    m_classNotesValidationText.Text({});
    m_classNotesValidationText.Visibility(Visibility::Collapsed);
    clearClassDirty();
    refreshClassRoster();
    refreshSpeakingEvaluation();
    refreshSpeakingAnalytics();
    refreshClassNavigation(true);
}

classmngr::engine::ClassInfo MainWindow::classInfoFromForm() const
{
    classmngr::engine::ClassInfo info = m_classInfo;
    info.classId = m_classSelectedId;
    info.classGrade = asUtf8(
        std::wstring_view(selectedComboValue(m_classGradeCombo))
        );
    info.classLevel = asUtf8(
        std::wstring_view(selectedComboValue(m_classLevelCombo))
        );
    info.readingBook = asUtf8(
        std::wstring_view(selectedComboValue(m_classReadingBookCombo))
        );
    info.essayBook = asUtf8(
        std::wstring_view(selectedComboValue(m_classEssayBookCombo))
        );
    info.classColor = asUtf8(m_classColorTextBox.Text());
    info.fontColor = asUtf8(m_classFontColorTextBox.Text());
    info.teacherId = m_classCoTeacherSelectedId;
    info.classTimes = classScheduleFromForm(false);
    info.intensiveTimes = classScheduleFromForm(true);
    info.notes = asUtf8(m_classNotesTextBox.Text());
    info.timeFillerActivities = asUtf8(
        m_classTimeFillerActivitiesTextBox.Text()
        );
    return info;
}

classmngr::engine::Roster MainWindow::classRosterFromForm() const
{
    classmngr::engine::Roster roster = m_classRoster;
    roster.rows.clear();
    roster.rows.reserve(m_classRosterCellBoxes.size());
    for (const auto& rowBoxes : m_classRosterCellBoxes)
    {
        std::vector<std::string> row;
        row.reserve(roster.columns.size());
        for (std::size_t column = 0; column < roster.columns.size(); ++column)
        {
            row.push_back(
                column < rowBoxes.size()
                    ? asUtf8(rowBoxes[column].Text())
                    : std::string{}
                );
        }
        roster.rows.push_back(std::move(row));
    }
    padRosterRows(roster);
    return roster;
}

} // namespace winrt::ClassMngrWinUI::implementation
