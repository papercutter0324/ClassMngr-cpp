#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::populateCalendarWorkspace(
    Microsoft::UI::Xaml::Controls::StackPanel const& calendarRoot
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (m_calendarTabs)
    {
        calendarRoot.Children().Append(m_calendarTabs);
        refreshCalendarPage();
        return;
    }

    const EngineCalendarDate today = calendarToday();
    m_calendarDisplayedMonth = calendarMonthStart(today);
    if (static_cast<int>(m_calendarDisplayedMonth.year())
        < calendarFirstTermYear)
    {
        m_calendarDisplayedMonth = EngineCalendarDate{
            std::chrono::year{calendarFirstTermYear},
            std::chrono::month{1},
            std::chrono::day{1}
        };
    }
    m_calendarSelectedDate = today.ok() && !calendarDateLess(
        today,
        classmngr::engine::AcademicCalendarSchedule::initialWinterStart()
        )
        ? today
        : m_calendarDisplayedMonth;

    auto makeText = [](std::wstring_view text, double fontSize = 0.0) {
        auto value = TextBlock();
        value.Text(winrt::hstring(text));
        value.TextWrapping(TextWrapping::Wrap);
        if (fontSize > 0.0)
        {
            value.FontSize(fontSize);
        }
        return value;
    };

    auto monthContent = StackPanel();
    monthContent.Padding(Thickness{16.0, 16.0, 16.0, 24.0});
    monthContent.Spacing(12.0);
    monthContent.HorizontalAlignment(HorizontalAlignment::Stretch);

    m_calendarMonthTitle = makeText(L"Calendar", 24.0);
    setAutomationName(m_calendarMonthTitle, L"Calendar month title");
    monthContent.Children().Append(m_calendarMonthTitle);

    auto monthToolbar = StackPanel();
    monthToolbar.Orientation(Orientation::Horizontal);
    monthToolbar.Spacing(8.0);

    m_calendarPreviousButton = Button();
    m_calendarPreviousButton.Content(box_value(hstring(L"Previous month")));
    m_calendarPreviousButton.Click({this, &MainWindow::CalendarPreviousButton_Click});
    setAutomationName(m_calendarPreviousButton, L"Calendar previous month");
    monthToolbar.Children().Append(m_calendarPreviousButton);

    m_calendarTodayButton = Button();
    m_calendarTodayButton.Content(box_value(hstring(L"Today")));
    m_calendarTodayButton.Click({this, &MainWindow::CalendarTodayButton_Click});
    setAutomationName(m_calendarTodayButton, L"Calendar today");
    monthToolbar.Children().Append(m_calendarTodayButton);

    m_calendarNextButton = Button();
    m_calendarNextButton.Content(box_value(hstring(L"Next month")));
    m_calendarNextButton.Click({this, &MainWindow::CalendarNextButton_Click});
    setAutomationName(m_calendarNextButton, L"Calendar next month");
    monthToolbar.Children().Append(m_calendarNextButton);

    m_calendarAddEventButton = Button();
    m_calendarAddEventButton.Content(box_value(hstring(L"Add event")));
    m_calendarAddEventButton.Click(
        [this](auto const&, auto const&) { openCalendarEventEditor(-1); }
        );
    setAutomationName(m_calendarAddEventButton, L"Calendar add event");
    monthToolbar.Children().Append(m_calendarAddEventButton);
    monthContent.Children().Append(monthToolbar);

    m_calendarGrid = Grid();
    m_calendarGrid.ColumnSpacing(4.0);
    m_calendarGrid.RowSpacing(4.0);
    m_calendarGrid.MinHeight(360.0);
    setAutomationName(m_calendarGrid, L"Calendar month grid");
    for (int column = 0; column < 7; ++column)
    {
        auto definition = ColumnDefinition();
        definition.Width(
            GridLengthHelper::FromValueAndType(1.0, GridUnitType::Star)
            );
        m_calendarGrid.ColumnDefinitions().Append(definition);
    }
    for (int row = 0; row < 7; ++row)
    {
        m_calendarGrid.RowDefinitions().Append(RowDefinition());
    }
    monthContent.Children().Append(m_calendarGrid);

    m_calendarSelectedDateText = makeText(L"Selected date", 18.0);
    setAutomationName(m_calendarSelectedDateText, L"Calendar selected date");
    monthContent.Children().Append(m_calendarSelectedDateText);

    m_calendarEventsPanel = StackPanel();
    m_calendarEventsPanel.Spacing(6.0);
    setAutomationName(m_calendarEventsPanel, L"Calendar selected day events");
    monthContent.Children().Append(m_calendarEventsPanel);

    m_calendarStatusText = makeText(L"Calendar is ready.");
    setAutomationName(m_calendarStatusText, L"Calendar status");
    monthContent.Children().Append(m_calendarStatusText);

    m_calendarValidationText = makeText(L"");
    m_calendarValidationText.Visibility(Visibility::Collapsed);
    setAutomationName(m_calendarValidationText, L"Calendar validation");
    monthContent.Children().Append(m_calendarValidationText);

    auto preferencesContent = StackPanel();
    preferencesContent.Padding(Thickness{16.0, 16.0, 16.0, 24.0});
    preferencesContent.Spacing(12.0);

    auto preferencesHeading = makeText(L"Calendar preferences", 24.0);
    setAutomationName(preferencesHeading, L"Calendar preferences heading");
    preferencesContent.Children().Append(preferencesHeading);

    m_calendarShowAllCampusesCheck = CheckBox();
    m_calendarShowAllCampusesCheck.Content(
        box_value(hstring(L"Show Events at All Campuses"))
        );
    setAutomationName(
        m_calendarShowAllCampusesCheck,
        L"Calendar show events at all campuses"
        );
    preferencesContent.Children().Append(m_calendarShowAllCampusesCheck);

    m_calendarHideStartOfTermCheck = CheckBox();
    m_calendarHideStartOfTermCheck.Content(
        box_value(hstring(L"Hide Start of Term Events"))
        );
    setAutomationName(
        m_calendarHideStartOfTermCheck,
        L"Calendar hide start of term events"
        );
    preferencesContent.Children().Append(m_calendarHideStartOfTermCheck);

    auto firstDayLabel = makeText(L"First day of calendar week");
    preferencesContent.Children().Append(firstDayLabel);
    m_calendarFirstDayCombo = ComboBox();
    auto sunday = ComboBoxItem();
    sunday.Content(box_value(hstring(L"Sunday")));
    sunday.Tag(box_value(hstring(L"0")));
    m_calendarFirstDayCombo.Items().Append(sunday);
    auto monday = ComboBoxItem();
    monday.Content(box_value(hstring(L"Monday")));
    monday.Tag(box_value(hstring(L"1")));
    m_calendarFirstDayCombo.Items().Append(monday);
    setAutomationName(m_calendarFirstDayCombo, L"Calendar first day of week");
    preferencesContent.Children().Append(m_calendarFirstDayCombo);

    auto termYearLabel = makeText(L"Academic term year");
    preferencesContent.Children().Append(termYearLabel);
    m_calendarTermYearTextBox = TextBox();
    m_calendarTermYearTextBox.PlaceholderText(L"2026");
    setAutomationName(m_calendarTermYearTextBox, L"Calendar academic term year");
    preferencesContent.Children().Append(m_calendarTermYearTextBox);

    auto scheduleHeading = makeText(
        L"Term schedules (use Monday dates and 1-53 week durations)",
        18.0
        );
    preferencesContent.Children().Append(scheduleHeading);

    auto scheduleGrid = Grid();
    scheduleGrid.ColumnSpacing(8.0);
    scheduleGrid.RowSpacing(6.0);
    for (int column = 0; column < 6; ++column)
    {
        auto definition = ColumnDefinition();
        if (column == 0)
        {
            definition.Width(
                GridLengthHelper::FromValueAndType(1.0, GridUnitType::Auto)
                );
        }
        else
        {
            definition.Width(
                GridLengthHelper::FromValueAndType(1.0, GridUnitType::Star)
                );
        }
        scheduleGrid.ColumnDefinitions().Append(definition);
    }
    for (int row = 0; row < 3; ++row)
    {
        scheduleGrid.RowDefinitions().Append(RowDefinition());
    }
    const std::array<std::wstring_view, 6> scheduleHeaders{
        L"School", L"Winter start", L"Winter weeks", L"Spring weeks",
        L"Summer weeks", L"Fall weeks"
    };
    for (int column = 0; column < 6; ++column)
    {
        auto header = makeText(scheduleHeaders[static_cast<std::size_t>(column)]);
        Grid::SetRow(header, 0);
        Grid::SetColumn(header, column);
        scheduleGrid.Children().Append(header);
    }
    const std::array<std::wstring_view, 2> schoolNames{
        L"Elementary", L"Middle"
    };
    for (int school = 0; school < 2; ++school)
    {
        auto schoolText = makeText(schoolNames[static_cast<std::size_t>(school)]);
        Grid::SetRow(schoolText, school + 1);
        Grid::SetColumn(schoolText, 0);
        scheduleGrid.Children().Append(schoolText);

        m_calendarWinterStartTextBoxes[static_cast<std::size_t>(school)] =
            TextBox();
        m_calendarWinterStartTextBoxes[static_cast<std::size_t>(school)].
            PlaceholderText(L"yyyy-MM-dd");
        setAutomationName(
            m_calendarWinterStartTextBoxes[static_cast<std::size_t>(school)],
            std::wstring(L"Calendar ") + std::wstring(schoolNames[static_cast<std::size_t>(school)])
                + L" winter start"
            );
        Grid::SetRow(
            m_calendarWinterStartTextBoxes[static_cast<std::size_t>(school)],
            school + 1
            );
        Grid::SetColumn(
            m_calendarWinterStartTextBoxes[static_cast<std::size_t>(school)],
            1
            );
        scheduleGrid.Children().Append(
            m_calendarWinterStartTextBoxes[static_cast<std::size_t>(school)]
            );

        for (int term = 0; term < classmngr::engine::AcademicTermCount; ++term)
        {
            auto field = TextBox();
            field.PlaceholderText(L"weeks");
            setAutomationName(
                field,
                std::wstring(L"Calendar ")
                    + std::wstring(schoolNames[static_cast<std::size_t>(school)])
                    + L" " + std::to_wstring(term + 1) + L" term weeks"
                );
            m_calendarTermWeekTextBoxes[static_cast<std::size_t>(school)]
                [static_cast<std::size_t>(term)] = field;
            Grid::SetRow(field, school + 1);
            Grid::SetColumn(field, term + 2);
            scheduleGrid.Children().Append(field);
        }
    }
    preferencesContent.Children().Append(scheduleGrid);

    auto preferencesActions = StackPanel();
    preferencesActions.Orientation(Orientation::Horizontal);
    preferencesActions.Spacing(8.0);
    m_calendarSavePreferencesButton = Button();
    m_calendarSavePreferencesButton.Content(box_value(hstring(L"Save preferences")));
    m_calendarSavePreferencesButton.Click(
        {this, &MainWindow::CalendarSavePreferencesButton_Click}
        );
    setAutomationName(
        m_calendarSavePreferencesButton,
        L"Calendar save preferences"
        );
    preferencesActions.Children().Append(m_calendarSavePreferencesButton);
    m_calendarRestoreDefaultsButton = Button();
    m_calendarRestoreDefaultsButton.Content(
        box_value(hstring(L"Restore term defaults"))
        );
    m_calendarRestoreDefaultsButton.Click(
        {this, &MainWindow::CalendarRestoreDefaultsButton_Click}
        );
    setAutomationName(
        m_calendarRestoreDefaultsButton,
        L"Calendar restore term defaults"
        );
    preferencesActions.Children().Append(m_calendarRestoreDefaultsButton);
    m_calendarResetEventsButton = Button();
    m_calendarResetEventsButton.Content(
        box_value(hstring(L"Reset calendar events"))
        );
    m_calendarResetEventsButton.Click(
        {this, &MainWindow::CalendarResetEventsButton_Click}
        );
    setAutomationName(m_calendarResetEventsButton, L"Calendar reset events");
    preferencesActions.Children().Append(m_calendarResetEventsButton);
    preferencesContent.Children().Append(preferencesActions);

    m_calendarPreferencesStatusText = makeText(L"Preferences are ready.");
    setAutomationName(
        m_calendarPreferencesStatusText,
        L"Calendar preferences status"
        );
    preferencesContent.Children().Append(m_calendarPreferencesStatusText);
    m_calendarPreferencesValidationText = makeText(L"");
    m_calendarPreferencesValidationText.Visibility(Visibility::Collapsed);
    setAutomationName(
        m_calendarPreferencesValidationText,
        L"Calendar preferences validation"
        );
    preferencesContent.Children().Append(m_calendarPreferencesValidationText);

    auto wrap = [](StackPanel const& content) {
        auto scroll = ScrollViewer();
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
        scroll.Content(content);
        return scroll;
    };
    auto calendarItem = PivotItem();
    calendarItem.Header(box_value(hstring(L"Calendar")));
    calendarItem.Content(wrap(monthContent));
    setAutomationName(calendarItem, L"Calendar month tab");
    auto preferencesItem = PivotItem();
    preferencesItem.Header(box_value(hstring(L"Preferences")));
    preferencesItem.Content(wrap(preferencesContent));
    setAutomationName(preferencesItem, L"Calendar preferences tab");

    m_calendarTabs = Pivot();
    m_calendarTabs.IsTabStop(true);
    m_calendarTabs.TabIndex(0);
    m_calendarTabs.Items().Append(calendarItem);
    m_calendarTabs.Items().Append(preferencesItem);
    setAutomationName(m_calendarTabs, L"Calendar tabs");
    calendarRoot.Children().Append(m_calendarTabs);
    refreshCalendarPage();
}

void MainWindow::refreshCalendarPage()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_calendarTabs || !m_calendarGrid || !m_calendarStatusText)
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
    setEnabled(m_calendarPreviousButton);
    setEnabled(m_calendarNextButton);
    setEnabled(m_calendarTodayButton);
    setEnabled(m_calendarAddEventButton);
    setEnabled(m_calendarShowAllCampusesCheck);
    setEnabled(m_calendarHideStartOfTermCheck);
    setEnabled(m_calendarFirstDayCombo);
    setEnabled(m_calendarTermYearTextBox);
    setEnabled(m_calendarSavePreferencesButton);
    setEnabled(m_calendarRestoreDefaultsButton);
    setEnabled(m_calendarResetEventsButton);
    for (auto const& field : m_calendarWinterStartTextBoxes)
    {
        setEnabled(field);
    }
    for (auto const& school : m_calendarTermWeekTextBoxes)
    {
        for (auto const& field : school)
        {
            setEnabled(field);
        }
    }

    if (!m_calendarDisplayedMonth.ok())
    {
        m_calendarDisplayedMonth = calendarMonthStart(calendarToday());
    }
    if (!m_calendarSelectedDate.ok())
    {
        m_calendarSelectedDate = m_calendarDisplayedMonth;
    }

    m_calendarLoading = true;
    m_calendarPreferencesDirty = false;
    m_calendarEvents.clear();
    m_calendarFirstDayOfWeek = 0;
    int termYear = std::max(
        calendarFirstTermYear,
        static_cast<int>(calendarToday().year())
        );

    if (!hasDatabase)
    {
        m_calendarStatusText.Text(L"No database open.");
        m_calendarSelectedDateText.Text(L"Selected date: ");
        m_calendarEventsPanel.Children().Clear();
        auto empty = TextBlock();
        empty.Text(L"Open a database to view and edit calendar events.");
        empty.TextWrapping(TextWrapping::Wrap);
        m_calendarEventsPanel.Children().Append(empty);
        m_calendarValidationText.Text({});
        m_calendarValidationText.Visibility(Visibility::Collapsed);
        m_calendarPreferencesStatusText.Text(L"No database open.");
        m_calendarPreferencesValidationText.Text({});
        m_calendarPreferencesValidationText.Visibility(Visibility::Collapsed);
        m_calendarLoading = false;
    }
    else
    {
        classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
        const auto loadSetting = [&settings](std::string_view key) {
            return settings.load(key);
        };
        if (const auto value = loadSetting("calendar/firstDayOfWeek"); value)
        {
            m_calendarFirstDayOfWeek = static_cast<int>(settingInteger(*value, 0));
        }
        m_calendarFirstDayOfWeek = m_calendarFirstDayOfWeek == 1 ? 1 : 0;
        if (const auto value = loadSetting("calendar/academic/termYear"); value)
        {
            termYear = static_cast<int>(settingInteger(*value, termYear));
        }
        termYear = std::max(calendarFirstTermYear, termYear);
        bool showAll = false;
        if (const auto value = loadSetting("calendar/showEventsAtAllCampuses"); value)
        {
            showAll = settingBool(*value, false);
        }
        bool hideStart = false;
        if (const auto value = loadSetting("calendar/hideStartOfTermEvents"); value)
        {
            hideStart = settingBool(*value, false);
        }
        m_calendarShowAllCampusesCheck.IsChecked(showAll);
        m_calendarHideStartOfTermCheck.IsChecked(hideStart);
        m_calendarFirstDayCombo.SelectedIndex(m_calendarFirstDayOfWeek);
        m_calendarTermYearTextBox.Text(std::to_wstring(termYear));

        m_calendarSchedule.clear();
        classmngr::engine::AcademicCalendarSchedule::ScheduleMap elementary;
        classmngr::engine::AcademicCalendarSchedule::ScheduleMap middle;
        for (int school = 0; school < 2; ++school)
        {
            const auto level = school == 0
                ? classmngr::engine::SchoolLevel::Elementary
                : classmngr::engine::SchoolLevel::Middle;
            auto schedule = m_calendarSchedule.defaultYearSchedule(level, termYear);
            bool hasCustom = false;
            const auto winter = loadSetting(calendarScheduleKey(
                termYear,
                school,
                0,
                true
                ));
            if (winter && std::holds_alternative<std::string>(*winter))
            {
                EngineCalendarDate parsed;
                if (calendarDateFromText(
                        asWString(winrt::to_hstring(
                            std::get<std::string>(*winter)
                            )),
                        parsed
                        ))
                {
                    schedule.winterStart = parsed;
                    hasCustom = true;
                }
            }
            for (int term = 0;
                 term < classmngr::engine::AcademicTermCount;
                 ++term)
            {
                const auto weeks = loadSetting(calendarScheduleKey(
                    termYear,
                    school,
                    term,
                    false
                    ));
                if (weeks)
                {
                    const auto value = settingInteger(*weeks, -1);
                    if (value >= 1 && value <= 53)
                    {
                        schedule.weeks[static_cast<std::size_t>(term)] =
                            static_cast<int>(value);
                        hasCustom = true;
                    }
                }
            }
            if (hasCustom && schedule.isValid())
            {
                (school == 0 ? elementary : middle).insert({termYear, schedule});
            }
        }
        static_cast<void>(m_calendarSchedule.replaceSchedules(elementary, middle));

        for (int school = 0; school < 2; ++school)
        {
            const auto level = school == 0
                ? classmngr::engine::SchoolLevel::Elementary
                : classmngr::engine::SchoolLevel::Middle;
            const auto schedule = m_calendarSchedule.yearSchedule(level, termYear);
            m_calendarWinterStartTextBoxes[static_cast<std::size_t>(school)].Text(
                calendarDateText(schedule.winterStart)
                );
            for (int term = 0;
                 term < classmngr::engine::AcademicTermCount;
                 ++term)
            {
                m_calendarTermWeekTextBoxes[static_cast<std::size_t>(school)]
                    [static_cast<std::size_t>(term)].Text(
                        std::to_wstring(schedule.weeks[static_cast<std::size_t>(term)])
                        );
            }
        }

        classmngr::engine::CalendarEventService service(*m_openDatabase);
        const auto loaded = service.loadInRange(
            calendarMonthStart(m_calendarDisplayedMonth),
            calendarAddDays(
                calendarMonthStart(m_calendarDisplayedMonth),
                calendarDaysInMonth(m_calendarDisplayedMonth) - 1
                )
            );
        if (loaded)
        {
            m_calendarEvents = *loaded;
            m_calendarStatusText.Text(winrt::hstring(
                L"Calendar loaded: " + std::to_wstring(m_calendarEvents.size())
                    + L" event(s)."
                ));
            m_calendarValidationText.Text({});
            m_calendarValidationText.Visibility(Visibility::Collapsed);
        }
        else
        {
            m_calendarStatusText.Text(winrt::hstring(
                L"Calendar could not be loaded: "
                    + asWide(loaded.error().message)
                ));
            m_calendarValidationText.Text(winrt::hstring(
                L"Engine loading error: " + asWide(loaded.error().message)
                ));
            m_calendarValidationText.Visibility(Visibility::Visible);
        }
        m_calendarPreferencesStatusText.Text(L"Calendar preferences loaded.");
        m_calendarPreferencesValidationText.Text({});
        m_calendarPreferencesValidationText.Visibility(Visibility::Collapsed);
        m_calendarLoading = false;
    }

    const auto visibleOnDate = [this](
                                  classmngr::engine::CalendarEvent const& event,
                                  EngineCalendarDate const& date) {
        if (!event.startDate.ok() || !event.endDate.ok()
            || calendarDateLess(date, event.startDate)
            || calendarDateLess(event.endDate, date))
        {
            return false;
        }
        const auto checked = m_calendarHideStartOfTermCheck.IsChecked();
        const bool hideStart = checked && checked.Value();
        return !hideStart || !classmngr::engine::CalendarEventRules::isStartOfTerm(
            event.title,
            event.eventType
            );
    };
    const auto eventSummary = [](classmngr::engine::CalendarEvent const& event) {
        std::wstring result = asWide(event.title);
        if (result.empty())
        {
            result = L"(untitled event)";
        }
        result += L" â€” " + asWide(event.eventType);
        if (event.allDay)
        {
            result += L" Â· All day";
        }
        else if (event.startTime)
        {
            result += L" Â· " + calendarTimeText(event.startTime);
            if (event.endTime)
            {
                result += L"-" + calendarTimeText(event.endTime);
            }
        }
        return result;
    };

    m_calendarMonthTitle.Text(calendarMonthTitle(m_calendarDisplayedMonth));
    const auto monthStart = calendarMonthStart(m_calendarDisplayedMonth);
    const int firstWeekday = static_cast<int>(
        std::chrono::weekday{std::chrono::sys_days{monthStart}}.c_encoding()
        );
    const int offset = (firstWeekday - m_calendarFirstDayOfWeek + 7) % 7;
    const auto gridStart = calendarAddDays(monthStart, -offset);
    const std::array<std::wstring_view, 7> weekdayNames{
        L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat"
    };
    m_calendarGrid.Children().Clear();
    for (int column = 0; column < 7; ++column)
    {
        auto header = TextBlock();
        header.Text(winrt::hstring(weekdayNames[static_cast<std::size_t>(
            (m_calendarFirstDayOfWeek + column) % 7
            )]));
        header.HorizontalAlignment(HorizontalAlignment::Center);
        setAutomationName(header, L"Calendar weekday header");
        Grid::SetRow(header, 0);
        Grid::SetColumn(header, column);
        m_calendarGrid.Children().Append(header);
    }
    for (int index = 0; index < 42; ++index)
    {
        const auto date = calendarAddDays(gridStart, index);
        int eventCount = 0;
        for (auto const& event : m_calendarEvents)
        {
            if (visibleOnDate(event, date))
            {
                ++eventCount;
            }
        }
        std::wstring content = std::to_wstring(
            static_cast<unsigned>(date.day())
            );
        if (eventCount > 0)
        {
            content += L"\nâ€¢ " + std::to_wstring(eventCount);
        }
        auto day = Button();
        day.Content(box_value(hstring(content)));
        day.MinHeight(48.0);
        day.IsEnabled(hasDatabase);
        day.Opacity(
            date.month() == m_calendarDisplayedMonth.month()
                ? 1.0
                : 0.55
            );
        setAutomationName(day, L"Calendar day " + calendarDateText(date));
        day.Click([this, date](auto const&, auto const&) {
            if (!m_calendarLoading)
            {
                m_calendarSelectedDate = date;
                refreshCalendarPage();
            }
        });
        Grid::SetRow(day, index / 7 + 1);
        Grid::SetColumn(day, index % 7);
        m_calendarGrid.Children().Append(day);
    }

    m_calendarSelectedDateText.Text(winrt::hstring(
        L"Selected date: " + calendarDateText(m_calendarSelectedDate)
        ));
    m_calendarEventsPanel.Children().Clear();
    int selectedEventCount = 0;
    for (auto const& event : m_calendarEvents)
    {
        if (!visibleOnDate(event, m_calendarSelectedDate))
        {
            continue;
        }
        ++selectedEventCount;
        auto eventButton = Button();
        eventButton.HorizontalAlignment(HorizontalAlignment::Stretch);
        eventButton.HorizontalContentAlignment(HorizontalAlignment::Left);
        eventButton.Content(box_value(hstring(eventSummary(event))));
        setAutomationName(
            eventButton,
            L"Calendar event " + std::to_wstring(event.id)
            );
        const int eventId = event.id;
        eventButton.Click(
            [this, eventId](auto const&, auto const&) {
                openCalendarEventEditor(eventId);
            }
            );
        m_calendarEventsPanel.Children().Append(eventButton);
    }
    if (selectedEventCount == 0)
    {
        auto empty = TextBlock();
        empty.Text(L"No events for the selected date.");
        empty.TextWrapping(TextWrapping::Wrap);
        setAutomationName(empty, L"Calendar no selected day events");
        m_calendarEventsPanel.Children().Append(empty);
    }

    const auto firstTermStart = classmngr::engine::AcademicCalendarSchedule::initialWinterStart();
    m_calendarPreviousButton.IsEnabled(
        hasDatabase && calendarDateLess(firstTermStart, monthStart)
        );
    m_calendarNextButton.IsEnabled(hasDatabase);
    m_calendarTodayButton.IsEnabled(hasDatabase);
}

} // namespace winrt::ClassMngrWinUI::implementation
