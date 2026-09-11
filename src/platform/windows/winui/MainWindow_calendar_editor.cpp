#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::CalendarPreviousButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        return;
    }
    const auto previous = calendarAddMonths(m_calendarDisplayedMonth, -1);
    if (previous.ok()
        && !calendarDateLess(
            previous,
            calendarMonthStart(classmngr::engine::AcademicCalendarSchedule::initialWinterStart())
            ))
    {
        m_calendarDisplayedMonth = previous;
        m_calendarSelectedDate = previous;
        refreshCalendarPage();
    }
}

void MainWindow::CalendarNextButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        return;
    }
    m_calendarDisplayedMonth = calendarAddMonths(m_calendarDisplayedMonth, 1);
    m_calendarSelectedDate = m_calendarDisplayedMonth;
    refreshCalendarPage();
}

void MainWindow::CalendarTodayButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        return;
    }
    const auto today = calendarToday();
    m_calendarDisplayedMonth = calendarMonthStart(today);
    if (calendarDateLess(
            today,
            classmngr::engine::AcademicCalendarSchedule::initialWinterStart()
            ))
    {
        m_calendarDisplayedMonth = calendarMonthStart(
            classmngr::engine::AcademicCalendarSchedule::initialWinterStart()
            );
    }
    m_calendarSelectedDate = today;
    refreshCalendarPage();
}

void MainWindow::saveCalendarPreferences()
{
    if (!m_openDatabase)
    {
        m_calendarPreferencesStatusText.Text(L"No database open.");
        return;
    }

    auto showValidation = [this](std::wstring_view message) {
        m_calendarPreferencesValidationText.Text(winrt::hstring(message));
        m_calendarPreferencesValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_calendarPreferencesStatusText.Text(L"Calendar preferences were not saved.");
    };
    int termYear = 0;
    try
    {
        termYear = std::stoi(asWString(m_calendarTermYearTextBox.Text()));
    }
    catch (...)
    {
        showValidation(L"Academic term year must be a number.");
        return;
    }
    if (termYear < calendarFirstTermYear || termYear > 2200)
    {
        showValidation(L"Academic term year must be between 2026 and 2200.");
        return;
    }

    std::array<classmngr::engine::AcademicYearSchedule, 2> schedules;
    for (int school = 0; school < 2; ++school)
    {
        auto& schedule = schedules[static_cast<std::size_t>(school)];
        schedule.termYear = termYear;
        if (!calendarDateFromText(
                asWString(m_calendarWinterStartTextBoxes[
                    static_cast<std::size_t>(school)
                    ].Text()),
                schedule.winterStart
                ))
        {
            showValidation(L"Each winter start must be a valid yyyy-MM-dd date.");
            return;
        }
        for (int term = 0;
             term < classmngr::engine::AcademicTermCount;
             ++term)
        {
            try
            {
                schedule.weeks[static_cast<std::size_t>(term)] = std::stoi(
                    asWString(m_calendarTermWeekTextBoxes[
                        static_cast<std::size_t>(school)
                        ][static_cast<std::size_t>(term)].Text())
                    );
            }
            catch (...)
            {
                showValidation(L"Each term duration must be a number from 1 to 53.");
                return;
            }
        }
        if (!schedule.isValid())
        {
            showValidation(
                L"Every term must start on a Monday and last from 1 to 53 weeks."
                );
            return;
        }
    }

    classmngr::engine::ApplicationSettings settingsValues;
    const auto checked = [](auto const& check) {
        const auto value = check.IsChecked();
        return value && value.Value();
    };
    settingsValues.emplace_back(
        "calendar/showEventsAtAllCampuses",
        classmngr::engine::SettingValue{
            std::int64_t{checked(m_calendarShowAllCampusesCheck) ? 1 : 0}
        }
        );
    settingsValues.emplace_back(
        "calendar/firstDayOfWeek",
        classmngr::engine::SettingValue{
            std::int64_t{m_calendarFirstDayCombo.SelectedIndex() == 1 ? 1 : 0}
        }
        );
    settingsValues.emplace_back(
        "calendar/hideStartOfTermEvents",
        classmngr::engine::SettingValue{
            std::int64_t{checked(m_calendarHideStartOfTermCheck) ? 1 : 0}
        }
        );
    settingsValues.emplace_back(
        "calendar/academic/termYear",
        classmngr::engine::SettingValue{std::int64_t{termYear}}
        );
    for (int school = 0; school < 2; ++school)
    {
        settingsValues.emplace_back(
            calendarScheduleKey(termYear, school, 0, true),
            classmngr::engine::SettingValue{
                asUtf8(calendarDateText(schedules[static_cast<std::size_t>(school)].winterStart))
            }
            );
        for (int term = 0;
             term < classmngr::engine::AcademicTermCount;
             ++term)
        {
            settingsValues.emplace_back(
                calendarScheduleKey(termYear, school, term, false),
                classmngr::engine::SettingValue{
                    std::int64_t{
                        schedules[static_cast<std::size_t>(school)]
                            .weeks[static_cast<std::size_t>(term)]
                    }
                }
                );
        }
    }

    classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
    const auto saved = settings.saveBatch(settingsValues);
    if (!saved)
    {
        showValidation(
            std::wstring(L"Engine rejected the calendar preferences: ")
                + asWide(saved.error().message)
            );
        return;
    }

    classmngr::engine::AcademicCalendarSchedule::ScheduleMap elementary;
    classmngr::engine::AcademicCalendarSchedule::ScheduleMap middle;
    elementary.insert({termYear, schedules[0]});
    middle.insert({termYear, schedules[1]});
    m_calendarSchedule.clear();
    static_cast<void>(m_calendarSchedule.replaceSchedules(elementary, middle));
    m_calendarFirstDayOfWeek = m_calendarFirstDayCombo.SelectedIndex() == 1 ? 1 : 0;
    m_calendarPreferencesDirty = false;
    m_dirtyState.markClean();
    m_calendarPreferencesValidationText.Text({});
    m_calendarPreferencesValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    m_calendarPreferencesStatusText.Text(L"Calendar preferences saved.");
    updateFileCommandState();
    refreshCalendarPage();
}

void MainWindow::restoreCalendarDefaults()
{
    if (!m_openDatabase)
    {
        m_calendarPreferencesStatusText.Text(L"No database open.");
        return;
    }

    int termYear = calendarFirstTermYear;
    try
    {
        termYear = std::max(
            calendarFirstTermYear,
            std::stoi(asWString(m_calendarTermYearTextBox.Text()))
            );
    }
    catch (...)
    {
        m_calendarTermYearTextBox.Text(std::to_wstring(termYear));
    }
    for (int school = 0; school < 2; ++school)
    {
        const auto level = school == 0
            ? classmngr::engine::SchoolLevel::Elementary
            : classmngr::engine::SchoolLevel::Middle;
        const auto schedule = m_calendarSchedule.defaultYearSchedule(level, termYear);
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
    m_calendarPreferencesDirty = true;
    m_calendarPreferencesStatusText.Text(
        L"Default term schedules loaded. Save preferences to persist them."
        );
}

void MainWindow::resetCalendarEvents()
{
    if (!m_openDatabase)
    {
        m_calendarPreferencesStatusText.Text(L"No database open.");
        return;
    }

    auto weak = get_weak();
    showDialog(
        L"Reset Calendar",
        L"Delete all calendar events? This cannot be undone.",
        L"Reset",
        {},
        L"Cancel",
        [weak](ClassMngrWinUIDialogs::DialogOutcome outcome) {
            if (outcome != ClassMngrWinUIDialogs::DialogOutcome::Primary)
            {
                return;
            }
            if (auto self = weak.get())
            {
                if (!self->m_openDatabase)
                {
                    return;
                }
                classmngr::engine::CalendarEventService service(
                    *self->m_openDatabase
                    );
                const auto deleted = service.removeAll();
                if (!deleted)
                {
                    self->m_calendarPreferencesStatusText.Text(winrt::hstring(
                        L"Calendar events could not be reset: "
                            + asWide(deleted.error().message)
                        ));
                    return;
                }
                self->m_dirtyState.markClean();
                self->m_calendarPreferencesStatusText.Text(
                    L"Calendar events reset to defaults."
                    );
                self->refreshCalendarPage();
                self->updateFileCommandState();
            }
        }
        );
}

void MainWindow::CalendarSavePreferencesButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    saveCalendarPreferences();
}

void MainWindow::CalendarRestoreDefaultsButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    restoreCalendarDefaults();
}

void MainWindow::CalendarResetEventsButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    resetCalendarEvents();
}

winrt::fire_and_forget MainWindow::openCalendarEventEditor(int eventId)
{
    auto lifetime = get_strong();
    if (m_ownedDialog || !m_openDatabase || !RootGrid().XamlRoot())
    {
        co_return;
    }

    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    classmngr::engine::CalendarEvent event;
    if (eventId > 0)
    {
        classmngr::engine::CalendarEventService service(*m_openDatabase);
        const auto loaded = service.get(eventId);
        if (!loaded)
        {
            m_calendarStatusText.Text(winrt::hstring(
                L"Calendar event could not be loaded: "
                    + asWide(loaded.error().message)
                ));
            co_return;
        }
        event = *loaded;
    }
    else
    {
        event.startDate = m_calendarSelectedDate.ok()
            ? m_calendarSelectedDate
            : calendarToday();
        if (calendarDateLess(
                event.startDate,
                classmngr::engine::AcademicCalendarSchedule::initialWinterStart()
                ))
        {
            event.startDate = classmngr::engine::AcademicCalendarSchedule::initialWinterStart();
        }
        event.endDate = event.startDate;
        event.startTime = std::chrono::minutes{9 * 60};
        event.endTime = std::chrono::minutes{10 * 60};
    }

    auto form = StackPanel();
    form.Spacing(8.0);
    form.MaxWidth(520.0);
    const auto makeField = [&form](
                                std::wstring_view header,
                                std::wstring value,
                                std::wstring_view automationName) {
        auto field = TextBox();
        field.Header(box_value(hstring(header)));
        field.Text(hstring(value));
        field.IsTabStop(true);
        setAutomationName(field, automationName);
        form.Children().Append(field);
        return field;
    };
    auto title = makeField(L"Title", asWide(event.title), L"Calendar event title");
    auto startDate = makeField(
        L"Start date (yyyy-MM-dd)",
        calendarDateText(event.startDate),
        L"Calendar event start date"
        );
    auto endDate = makeField(
        L"End date (yyyy-MM-dd)",
        calendarDateText(event.endDate),
        L"Calendar event end date"
        );
    auto startTime = makeField(
        L"Start time (HH:mm)",
        calendarTimeText(event.startTime),
        L"Calendar event start time"
        );
    auto endTime = makeField(
        L"End time (HH:mm)",
        calendarTimeText(event.endTime),
        L"Calendar event end time"
        );

    auto allDay = CheckBox();
    allDay.Content(box_value(hstring(L"All day")));
    allDay.IsChecked(event.allDay);
    setAutomationName(allDay, L"Calendar event all day");
    form.Children().Append(allDay);

    const auto addChoice = [](ComboBox combo,
                              std::wstring_view display,
                              std::wstring_view value) {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(display)));
        item.Tag(box_value(hstring(value)));
        combo.Items().Append(item);
    };
    const auto selectChoice = [](ComboBox combo, std::wstring_view value) {
        for (int index = 0; index < static_cast<int>(combo.Items().Size()); ++index)
        {
            const auto item = combo.Items().GetAt(index).try_as<ComboBoxItem>();
            if (item && boxedString(item.Tag()) == value)
            {
                combo.SelectedIndex(index);
                return;
            }
        }
        combo.SelectedIndex(0);
    };

    auto eventType = ComboBox();
    eventType.Header(box_value(hstring(L"Event type")));
    for (const auto value : classmngr::engine::CalendarEventRules::eventTypes())
    {
        addChoice(eventType, asWide(value), asWide(value));
    }
    selectChoice(eventType, asWide(event.eventType));
    setAutomationName(eventType, L"Calendar event type");
    form.Children().Append(eventType);

    auto timeStatus = ComboBox();
    timeStatus.Header(box_value(hstring(L"Time status")));
    for (const auto value : classmngr::engine::CalendarEventRules::timeStatuses())
    {
        addChoice(timeStatus, asWide(value), asWide(value));
    }
    selectChoice(timeStatus, asWide(event.timeStatus));
    setAutomationName(timeStatus, L"Calendar event time status");
    form.Children().Append(timeStatus);

    auto repeat = ComboBox();
    repeat.Header(box_value(hstring(L"Repeat")));
    addChoice(repeat, L"Does not repeat", L"none");
    addChoice(repeat, L"Daily", L"daily");
    addChoice(repeat, L"Weekly", L"weekly");
    addChoice(repeat, L"Monthly", L"monthly");
    selectChoice(repeat, L"none");
    setAutomationName(repeat, L"Calendar event repeat frequency");
    form.Children().Append(repeat);
    auto repeatUntil = makeField(
        L"Repeat until (yyyy-MM-dd; required for repeats)",
        {},
        L"Calendar event repeat until"
        );

    auto validation = TextBlock();
    validation.TextWrapping(TextWrapping::Wrap);
    validation.Visibility(Visibility::Collapsed);
    setAutomationName(validation, L"Calendar event validation");
    form.Children().Append(validation);

    auto dialog = ContentDialog();
    dialog.XamlRoot(RootGrid().XamlRoot());
    dialog.Title(box_value(hstring(
        eventId > 0 ? L"Edit calendar event" : L"Add calendar event"
        )));
    dialog.Content(form);
    dialog.PrimaryButtonText(L"Save");
    if (eventId > 0)
    {
        dialog.SecondaryButtonText(L"Delete");
    }
    dialog.CloseButtonText(L"Cancel");
    dialog.DefaultButton(ContentDialogButton::Primary);
    m_ownedDialog = dialog;

    for (;;)
    {
        ContentDialogResult result = ContentDialogResult::None;
        try
        {
            result = co_await dialog.ShowAsync();
        }
        catch (...)
        {
            break;
        }
        if (result == ContentDialogResult::None
            || result == ContentDialogResult::Secondary)
        {
            if (result == ContentDialogResult::Secondary && eventId > 0)
            {
                classmngr::engine::CalendarEventService service(*m_openDatabase);
                const auto deleted = service.remove(eventId);
                if (!deleted)
                {
                    m_calendarStatusText.Text(winrt::hstring(
                        L"Calendar event could not be deleted: "
                            + asWide(deleted.error().message)
                        ));
                }
                else
                {
                    m_calendarStatusText.Text(L"Calendar event deleted.");
                    m_dirtyState.markClean();
                    refreshCalendarPage();
                    updateFileCommandState();
                }
            }
            break;
        }

        classmngr::engine::CalendarEvent draft = event;
        draft.title = asUtf8(asWString(title.Text()));
        if (!calendarDateFromText(asWString(startDate.Text()), draft.startDate)
            || !calendarDateFromText(asWString(endDate.Text()), draft.endDate))
        {
            validation.Text(L"Start and end dates must use yyyy-MM-dd.");
            validation.Visibility(Visibility::Visible);
            continue;
        }
        const auto startTimeText = asWString(startTime.Text());
        const auto endTimeText = asWString(endTime.Text());
        const auto parsedStartTime = startTimeText.empty()
            ? std::optional<std::chrono::minutes>{}
            : calendarTimeFromText(startTimeText);
        const auto parsedEndTime = endTimeText.empty()
            ? std::optional<std::chrono::minutes>{}
            : calendarTimeFromText(endTimeText);
        if ((!startTimeText.empty() && !parsedStartTime)
            || (!endTimeText.empty() && !parsedEndTime))
        {
            validation.Text(L"Times must use HH:mm.");
            validation.Visibility(Visibility::Visible);
            continue;
        }
        draft.startTime = parsedStartTime;
        draft.endTime = parsedEndTime;
        const auto allDayValue = allDay.IsChecked();
        draft.allDay = allDayValue && allDayValue.Value();
        draft.eventType = asUtf8(selectedComboValue(eventType));
        draft.timeStatus = asUtf8(selectedComboValue(timeStatus));
        if (draft.allDay)
        {
            draft.timeStatus = "Timed";
            draft.startTime.reset();
            draft.endTime.reset();
        }
        const std::wstring repeatValue = selectedComboValue(repeat);
        const bool repeating = repeatValue != L"none";
        if (!repeating)
        {
            draft.repeatSeriesId.clear();
        }
        EngineCalendarDate repeatEnd;
        if (repeating)
        {
            if (!calendarDateFromText(asWString(repeatUntil.Text()), repeatEnd))
            {
                validation.Text(L"Repeat until must use yyyy-MM-dd.");
                validation.Visibility(Visibility::Visible);
                continue;
            }
        }

        const auto eventValidation =
            classmngr::engine::CalendarEventValidator::validate(draft);
        if (!eventValidation.isValid())
        {
            std::wstring message = L"Calendar event validation: ";
            for (const auto& issue : eventValidation.issues())
            {
                if (!issue.isError())
                {
                    continue;
                }
                if (message.back() != L' ')
                {
                    message += L"; ";
                }
                message += asWide(issue.code);
                if (!issue.field.empty())
                {
                    message += L" (" + asWide(issue.field) + L")";
                }
            }
            validation.Text(winrt::hstring(message));
            validation.Visibility(Visibility::Visible);
            continue;
        }
        classmngr::engine::CalendarEventService service(*m_openDatabase);
        bool persisted = false;
        std::string errorMessage;
        if (repeating)
        {
            classmngr::engine::CalendarEventRepeatFrequency frequency =
                classmngr::engine::CalendarEventRepeatFrequency::Daily;
            if (repeatValue == L"weekly")
            {
                frequency = classmngr::engine::CalendarEventRepeatFrequency::Weekly;
            }
            else if (repeatValue == L"monthly")
            {
                frequency = classmngr::engine::CalendarEventRepeatFrequency::Monthly;
            }
            const auto recurrenceValidation =
                classmngr::engine::CalendarEventValidator::validateRecurrence(
                    draft,
                    frequency,
                    repeatEnd
                    );
            if (!recurrenceValidation.isValid())
            {
                validation.Text(L"Repeat range is invalid or too long.");
                validation.Visibility(Visibility::Visible);
                continue;
            }
            if (eventId > 0 && !event.repeatSeriesId.empty())
            {
                const auto updated = service.updateRepeatSeriesFromDate(
                    event,
                    draft
                    );
                persisted = static_cast<bool>(updated);
                if (!persisted)
                {
                    errorMessage = updated.error().message;
                }
            }
            else if (eventId <= 0)
            {
                const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
                draft.repeatSeriesId = "winui-" + std::to_string(stamp);
                const auto created = service.createRepeatSeries(
                    draft,
                    frequency,
                    repeatEnd
                    );
                persisted = static_cast<bool>(created);
                if (!persisted)
                {
                    errorMessage = created.error().message;
                }
            }
            else
            {
                const auto saved = service.save(draft);
                persisted = static_cast<bool>(saved);
                if (!persisted)
                {
                    errorMessage = saved.error().message;
                }
            }
        }
        else
        {
            const auto saved = service.save(draft);
            persisted = static_cast<bool>(saved);
            if (!persisted)
            {
                errorMessage = saved.error().message;
            }
        }
        if (!persisted)
        {
            validation.Text(winrt::hstring(
                L"Calendar event could not be saved: " + asWide(errorMessage)
                ));
            validation.Visibility(Visibility::Visible);
            continue;
        }
        m_dirtyState.markClean();
        m_calendarStatusText.Text(L"Calendar event saved.");
        refreshCalendarPage();
        updateFileCommandState();
        break;
    }

    if (m_ownedDialog == dialog)
    {
        m_ownedDialog = nullptr;
    }
}

} // namespace winrt::ClassMngrWinUI::implementation
