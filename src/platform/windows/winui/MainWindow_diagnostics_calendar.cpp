#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

bool MainWindow::runPhase6CalendarChecks()
{
    m_phase6CalendarFailureMask = 0;
    const auto fail = [this](uint32_t failureMask) {
        m_phase6CalendarFailureMask = failureMask;
        return false;
    };

    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    navigateTo(homePageId);
    refreshCalendarPage();
    const bool noDatabaseReady =
        m_calendarTabs
        && m_calendarTabs.Items().Size() == 2
        && m_calendarGrid
        && m_calendarGrid.Children().Size() == 49
        && m_calendarStatusText.Text() == L"No database open."
        && !m_calendarAddEventButton.IsEnabled()
        && !m_calendarSavePreferencesButton.IsEnabled();
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
    m_calendarDisplayedMonth = EngineCalendarDate{
        std::chrono::year{calendarFirstTermYear},
        std::chrono::month{1},
        std::chrono::day{1}
    };
    m_calendarSelectedDate = EngineCalendarDate{
        std::chrono::year{calendarFirstTermYear},
        std::chrono::month{1},
        std::chrono::day{15}
    };
    refreshCalendarPage();
    const bool emptyReady =
        m_calendarStatusText.Text() == L"Calendar loaded: 0 event(s)."
        && m_calendarAddEventButton.IsEnabled()
        && m_calendarShowAllCampusesCheck.IsEnabled();
    if (!emptyReady)
    {
        return fail(3);
    }

    classmngr::engine::CalendarEvent event;
    event.title = "Calendar smoke event";
    event.eventType = "Meeting";
    event.startDate = m_calendarSelectedDate;
    event.endDate = m_calendarSelectedDate;
    event.startTime = std::chrono::minutes{9 * 60};
    event.endTime = std::chrono::minutes{10 * 60};
    classmngr::engine::CalendarEventService service(*m_openDatabase);
    const auto created = service.save(event);
    if (!created)
    {
        return fail(4);
    }
    refreshCalendarPage();
    const bool eventReady =
        m_calendarEvents.size() == 1
        && m_calendarEventsPanel.Children().Size() == 1
        && m_calendarStatusText.Text() == L"Calendar loaded: 1 event(s).";
    if (!eventReady)
    {
        return fail(5);
    }

    classmngr::engine::CalendarEvent invalid = event;
    invalid.endTime = invalid.startTime;
    const bool validationReady =
        !classmngr::engine::CalendarEventValidator::validate(invalid).isValid();
    if (!validationReady)
    {
        return fail(6);
    }

    m_calendarShowAllCampusesCheck.IsChecked(true);
    m_calendarHideStartOfTermCheck.IsChecked(true);
    m_calendarFirstDayCombo.SelectedIndex(1);
    saveCalendarPreferences();
    classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
    const auto firstDay = settings.load("calendar/firstDayOfWeek");
    const auto showAll = settings.load("calendar/showEventsAtAllCampuses");
    const auto hideStart = settings.load("calendar/hideStartOfTermEvents");
    const bool preferencesReady =
        firstDay && showAll && hideStart
        && settingInteger(*firstDay, -1) == 1
        && settingInteger(*showAll, -1) == 1
        && settingInteger(*hideStart, -1) == 1
        && m_calendarFirstDayOfWeek == 1;
    if (!preferencesReady)
    {
        uint32_t preferenceFailureMask = 7;
        if (!firstDay || settingInteger(*firstDay, -1) != 1)
        {
            preferenceFailureMask |= 0x100;
        }
        if (!showAll || settingInteger(*showAll, -1) != 1)
        {
            preferenceFailureMask |= 0x200;
        }
        if (!hideStart || settingInteger(*hideStart, -1) != 1)
        {
            preferenceFailureMask |= 0x400;
        }
        if (m_calendarFirstDayOfWeek != 1)
        {
            preferenceFailureMask |= 0x800;
        }
        if (m_calendarPreferencesStatusText.Text()
            == L"Calendar preferences were not saved.")
        {
            preferenceFailureMask |= 0x1000;
        }
        return fail(preferenceFailureMask);
    }

    const auto beforeNext = m_calendarMonthTitle.Text();
    CalendarNextButton_Click(
        m_calendarNextButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    const bool navigationReady = beforeNext != m_calendarMonthTitle.Text()
        && m_calendarDisplayedMonth.month() == std::chrono::month{2};
    CalendarPreviousButton_Click(
        m_calendarPreviousButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    if (!navigationReady || m_calendarDisplayedMonth.month() != std::chrono::month{1})
    {
        return fail(8);
    }

    const auto deleted = service.removeAll();
    if (!deleted)
    {
        return fail(9);
    }
    refreshCalendarPage();
    const bool resetReady = m_calendarEvents.empty()
        && m_calendarStatusText.Text() == L"Calendar loaded: 0 event(s).";
    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    refreshCalendarPage();
    const bool clearReady = m_calendarStatusText.Text() == L"No database open."
        && !m_calendarAddEventButton.IsEnabled();
    return resetReady && clearReady;
}

uint32_t MainWindow::phase6CalendarFailureMask() const noexcept
{
    return m_phase6CalendarFailureMask;
}

} // namespace winrt::ClassMngrWinUI::implementation
