#pragma once

#include "next/application/calendar_event_by_id_query_port.h"
#include "next/application/calendar_event_delete_all_port.h"
#include "next/application/calendar_event_delete_port.h"
#include "next/application/calendar_event_display_preferences.h"
#include "next/application/calendar_event_save_port.h"
#include "next/application/calendar_event_series_create_port.h"
#include "next/application/calendar_event_series_create_use_case.h"
#include "next/application/calendar_event_series_delete_port.h"
#include "next/application/calendar_event_series_edit_port.h"
#include "next/application/calendar_event_type_color_preferences.h"
#include "next/application/academic_calendar_schedule_preferences.h"
#include "next/application/calendar_first_day_of_week_preferences.h"
#include "next/application/current_campus_preferences.h"

#include <QString>

namespace ClassMngr::Next::Platform
{

[[nodiscard]] Application::CalendarEventByIdQueryResult
readCalendarEventAtPath(
    const QString& databasePath,
    const Domain::CalendarEventId& eventId
    );

[[nodiscard]] Application::CalendarEventSaveResult saveCalendarEventAtPath(
    const QString& databasePath,
    const Application::CalendarEventSaveInput& input
    );

[[nodiscard]] Application::CalendarEventDeleteResult deleteCalendarEventAtPath(
    const QString& databasePath,
    const Domain::CalendarEventId& eventId
    );

[[nodiscard]] Application::CalendarEventSeriesCreateResult
createCalendarEventSeriesAtPath(
    const QString& databasePath,
    const Application::CalendarEventSeriesCreateInput& input
    );

[[nodiscard]] Application::CalendarEventSeriesEditResult
editCalendarEventSeriesAtPath(
    const QString& databasePath,
    const Application::CalendarEventSeriesEditInput& input
    );

[[nodiscard]] Application::CalendarEventSeriesDeleteResult
deleteCalendarEventSeriesAtPath(
    const QString& databasePath,
    const Application::CalendarEventSeriesDeleteInput& input
    );

[[nodiscard]] Application::CalendarEventDeleteAllResult
deleteAllCalendarEventsAtPath(const QString& databasePath);

[[nodiscard]] Application::CalendarEventDisplayPreferencesResult
readCalendarEventDisplayPreferencesAtPath(const QString& databasePath);

[[nodiscard]] Application::CalendarEventDisplayPreferencesSaveResult
saveCalendarEventDisplayPreferencesAtPath(
    const QString& databasePath,
    const Application::CalendarEventDisplayPreferencesSaveRequest& request
    );

[[nodiscard]] Application::CalendarFirstDayOfWeekPreferencesLoadResult
readCalendarFirstDayOfWeekAtPath(const QString& databasePath);

[[nodiscard]] Application::CalendarFirstDayOfWeekPreferencesPort::SaveResult
saveCalendarFirstDayOfWeekAtPath(
    const QString& databasePath,
    Application::CalendarFirstDayOfWeek firstDayOfWeek
    );

[[nodiscard]] Application::AcademicCalendarSchedulePreferencesReadResult
readAcademicCalendarScheduleAtPath(const QString& databasePath);

[[nodiscard]] Application::AcademicCalendarSchedulePreferencesSaveResult
writeAcademicCalendarScheduleAtPath(
    const QString& databasePath,
    const Application::AcademicCalendarScheduleJsonText& payload
    );

[[nodiscard]] Domain::Result<Application::CurrentCampusPreferenceValue>
readCurrentCampusAtPath(const QString& databasePath);

[[nodiscard]] Domain::Result<std::string> readCalendarEventTypeColorAtPath(
    const QString& databasePath,
    Domain::CalendarEventType eventType
    );

[[nodiscard]] Application::CalendarEventTypeColorPreferencesPort::SaveResult
writeCalendarEventTypeColorAtPath(
    const QString& databasePath,
    Domain::CalendarEventType eventType,
    const std::string& colorHexRgb
    );

} // namespace ClassMngr::Next::Platform
