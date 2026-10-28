#include "next/platform/workspace_store_calendar_actions.h"

#include "data/database/workspace_store.h"
#include "next/application/academic_calendar_schedule_preferences.h"
#include "next/application/calendar_event_by_id_query_use_case.h"
#include "next/application/calendar_event_delete_all_use_case.h"
#include "next/application/calendar_event_delete_use_case.h"
#include "next/application/calendar_event_save_use_case.h"
#include "next/application/calendar_event_series_create_use_case.h"
#include "next/application/calendar_event_series_delete_use_case.h"
#include "next/application/calendar_event_series_edit_use_case.h"
#include "next/platform/application_services_academic_calendar_schedule_preferences_port.h"
#include "next/platform/application_services_calendar_event_delete_all_port.h"
#include "next/platform/application_services_calendar_event_delete_port.h"
#include "next/platform/application_services_calendar_event_display_preferences_port.h"
#include "next/platform/application_services_calendar_event_port.h"
#include "next/platform/application_services_calendar_event_save_port.h"
#include "next/platform/application_services_calendar_event_series_create_port.h"
#include "next/platform/application_services_calendar_event_series_delete_port.h"
#include "next/platform/application_services_calendar_event_series_edit_port.h"
#include "next/platform/application_services_calendar_event_type_color_preferences_port.h"
#include "next/platform/application_services_calendar_first_day_of_week_preferences_port.h"
#include "next/platform/application_services_current_campus_preferences_port.h"
#include "next/platform/repository_error_code.h"

#include <QByteArray>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{
namespace
{

using namespace Application;

template <typename Result, typename Operation>
[[nodiscard]] Result withWorkspace(
    const QString& databasePath,
    const char* failureMessage,
    Operation&& operation
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        const QByteArray bytes = toLegacyDiagnostic(opened.error()).toUtf8();
        return Result::failure({
            .code = repositoryDomainErrorCode(opened.error().code),
            .message = bytes.isEmpty() ? failureMessage : bytes.toStdString(),
            .recoverable = opened.error().recoverableOverride.value_or(
                opened.error().code != RepositoryErrorCode::Technical)
        });
    }

    try
    {
        return operation(store);
    }
    catch (const std::exception&)
    {
        return Result::failure({
            .code = Domain::ErrorCode::Technical,
            .message = failureMessage,
            .recoverable = false
        });
    }
    catch (...)
    {
        return Result::failure({
            .code = Domain::ErrorCode::Technical,
            .message = failureMessage,
            .recoverable = false
        });
    }
}

template <typename Port, typename Operation>
[[nodiscard]] auto withPort(
    const QString& databasePath,
    const char* failureMessage,
    Operation&& operation
    )
{
    using Result = decltype(operation(std::declval<Port&>()));
    return withWorkspace<Result>(
        databasePath,
        failureMessage,
        [&operation](WorkspaceStore& store) -> Result
        {
            Port port(store);
            return operation(port);
        }
        );
}

} // namespace

CalendarEventByIdQueryResult readCalendarEventAtPath(
    const QString& databasePath,
    const Domain::CalendarEventId& eventId
    )
{
    return withPort<ApplicationServicesCalendarEventPort>(
        databasePath,
        "Calendar event could not be loaded.",
        [&eventId](ApplicationServicesCalendarEventPort& port)
        {
            return CalendarEventByIdQueryUseCase::execute(
                port,
                CalendarEventByIdQueryRequest{.eventId = eventId}
                );
        }
        );
}

CalendarEventSaveResult saveCalendarEventAtPath(
    const QString& databasePath,
    const CalendarEventSaveInput& input
    )
{
    return withPort<ApplicationServicesCalendarEventSavePort>(
        databasePath,
        "Calendar event could not be saved.",
        [&input](ApplicationServicesCalendarEventSavePort& port)
        {
            return CalendarEventSaveUseCase::execute(port, input);
        }
        );
}

CalendarEventDeleteResult deleteCalendarEventAtPath(
    const QString& databasePath,
    const Domain::CalendarEventId& eventId
    )
{
    return withPort<ApplicationServicesCalendarEventDeletePort>(
        databasePath,
        "Calendar event could not be deleted.",
        [&eventId](ApplicationServicesCalendarEventDeletePort& port)
        {
            return CalendarEventDeleteUseCase::execute(port, eventId);
        }
        );
}

CalendarEventSeriesCreateResult createCalendarEventSeriesAtPath(
    const QString& databasePath,
    const CalendarEventSeriesCreateInput& input
    )
{
    return withPort<ApplicationServicesCalendarEventSeriesCreatePort>(
        databasePath,
        "Calendar repeat series could not be created.",
        [&input](ApplicationServicesCalendarEventSeriesCreatePort& port)
        {
            return CalendarEventSeriesCreateUseCase::execute(port, input);
        }
        );
}

CalendarEventSeriesEditResult editCalendarEventSeriesAtPath(
    const QString& databasePath,
    const CalendarEventSeriesEditInput& input
    )
{
    return withPort<ApplicationServicesCalendarEventSeriesEditPort>(
        databasePath,
        "Calendar repeat series could not be edited.",
        [&input](ApplicationServicesCalendarEventSeriesEditPort& port)
        {
            return CalendarEventSeriesEditUseCase::execute(port, input);
        }
        );
}

CalendarEventSeriesDeleteResult deleteCalendarEventSeriesAtPath(
    const QString& databasePath,
    const CalendarEventSeriesDeleteInput& input
    )
{
    return withPort<ApplicationServicesCalendarEventSeriesDeletePort>(
        databasePath,
        "Calendar repeat series could not be deleted.",
        [&input](ApplicationServicesCalendarEventSeriesDeletePort& port)
        {
            return CalendarEventSeriesDeleteUseCase::execute(port, input);
        }
        );
}

CalendarEventDeleteAllResult deleteAllCalendarEventsAtPath(
    const QString& databasePath
    )
{
    return withPort<ApplicationServicesCalendarEventDeleteAllPort>(
        databasePath,
        "Calendar events could not be reset.",
        [](ApplicationServicesCalendarEventDeleteAllPort& port)
        {
            return CalendarEventDeleteAllUseCase::execute(port);
        }
        );
}

CalendarEventDisplayPreferencesResult
readCalendarEventDisplayPreferencesAtPath(const QString& databasePath)
{
    return withPort<ApplicationServicesCalendarEventDisplayPreferencesPort>(
        databasePath,
        "Calendar display preferences could not be loaded.",
        [](ApplicationServicesCalendarEventDisplayPreferencesPort& port)
        {
            return port.load();
        }
        );
}

CalendarEventDisplayPreferencesSaveResult
saveCalendarEventDisplayPreferencesAtPath(
    const QString& databasePath,
    const CalendarEventDisplayPreferencesSaveRequest& request
    )
{
    return withPort<ApplicationServicesCalendarEventDisplayPreferencesPort>(
        databasePath,
        "Calendar display preferences could not be saved.",
        [&request](ApplicationServicesCalendarEventDisplayPreferencesPort& port)
        {
            return port.save(request);
        }
        );
}

CalendarFirstDayOfWeekPreferencesLoadResult readCalendarFirstDayOfWeekAtPath(
    const QString& databasePath
    )
{
    return withPort<ApplicationServicesCalendarFirstDayOfWeekPreferencesPort>(
        databasePath,
        "Calendar week-start preference could not be loaded.",
        [](ApplicationServicesCalendarFirstDayOfWeekPreferencesPort& port)
        {
            return port.load();
        }
        );
}

CalendarFirstDayOfWeekPreferencesPort::SaveResult
saveCalendarFirstDayOfWeekAtPath(
    const QString& databasePath,
    const CalendarFirstDayOfWeek firstDayOfWeek
    )
{
    return withPort<ApplicationServicesCalendarFirstDayOfWeekPreferencesPort>(
        databasePath,
        "Calendar week-start preference could not be saved.",
        [firstDayOfWeek](ApplicationServicesCalendarFirstDayOfWeekPreferencesPort& port)
        {
            return port.save(firstDayOfWeek);
        }
        );
}

AcademicCalendarSchedulePreferencesReadResult
readAcademicCalendarScheduleAtPath(const QString& databasePath)
{
    return withPort<ApplicationServicesAcademicCalendarSchedulePreferencesPort>(
        databasePath,
        "Academic calendar schedule could not be loaded.",
        [](ApplicationServicesAcademicCalendarSchedulePreferencesPort& port)
        {
            return port.read();
        }
        );
}

AcademicCalendarSchedulePreferencesSaveResult
writeAcademicCalendarScheduleAtPath(
    const QString& databasePath,
    const AcademicCalendarScheduleJsonText& payload
    )
{
    return withPort<ApplicationServicesAcademicCalendarSchedulePreferencesPort>(
        databasePath,
        "Academic calendar schedule could not be saved.",
        [&payload](ApplicationServicesAcademicCalendarSchedulePreferencesPort& port)
        {
            return port.write(payload);
        }
        );
}

Domain::Result<CurrentCampusPreferenceValue> readCurrentCampusAtPath(
    const QString& databasePath
    )
{
    return withPort<ApplicationServicesCurrentCampusPreferencesPort>(
        databasePath,
        "Current campus preference could not be loaded.",
        [](ApplicationServicesCurrentCampusPreferencesPort& port)
        {
            return port.read();
        }
        );
}

Domain::Result<std::string> readCalendarEventTypeColorAtPath(
    const QString& databasePath,
    const Domain::CalendarEventType eventType
    )
{
    return withPort<ApplicationServicesCalendarEventTypeColorPreferencesPort>(
        databasePath,
        "Calendar event color could not be loaded.",
        [eventType](ApplicationServicesCalendarEventTypeColorPreferencesPort& port)
        {
            return Domain::Result<std::string>::success(port.read(eventType));
        }
        );
}

CalendarEventTypeColorPreferencesPort::SaveResult
writeCalendarEventTypeColorAtPath(
    const QString& databasePath,
    const Domain::CalendarEventType eventType,
    const std::string& colorHexRgb
    )
{
    return withPort<ApplicationServicesCalendarEventTypeColorPreferencesPort>(
        databasePath,
        "Calendar event color could not be saved.",
        [eventType, &colorHexRgb](ApplicationServicesCalendarEventTypeColorPreferencesPort& port)
        {
            return port.write(eventType, colorHexRgb);
        }
        );
}

} // namespace ClassMngr::Next::Platform
