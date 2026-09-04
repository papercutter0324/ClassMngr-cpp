#pragma once

#include "core/result.h"
#include "domain/models/calendar_event.h"

#include <QList>
#include <QSqlDatabase>

#include <memory>

namespace classmngr::engine
{
class SqliteDatabase;
}

class CalendarEventRepository
{
public:
    explicit CalendarEventRepository(const QString& databasePath);
    // Compatibility-only constructor for retained Qt SQL tests/adapters.
    explicit CalendarEventRepository(
        QSqlDatabase& database
        );
    ~CalendarEventRepository();

    [[nodiscard]] Result<QList<CalendarEvent>> loadCalendarEventsForDate(
        const QDate& date
        );

    [[nodiscard]] Result<QList<CalendarEvent>> loadCalendarEventsInRange(
        const QDate& startDate,
        const QDate& endDate
        );

    [[nodiscard]] Result<QList<CalendarEvent>> loadUpcomingCalendarEvents(
        const QDate& fromDate,
        int limit
        );

    [[nodiscard]] Result<QDate> findNextCalendarEventStartDate(
        const QDate& fromDate
        );

    [[nodiscard]] Result<CalendarEvent> getCalendarEvent(
        int eventId
        );

    [[nodiscard]] Result<QList<CalendarEvent>> loadCalendarEventsForRepeatSeriesFromDate(
        const QString& repeatSeriesId,
        const QDate& startDate
        );

    [[nodiscard]] Result<QList<CalendarEvent>> expandRepeatSeries(
        const CalendarEvent& event,
        CalendarEventRepeatFrequency frequency,
        const QDate& untilDate
        );

    [[nodiscard]] Result<QList<int>> createRepeatSeries(
        const CalendarEvent& event,
        CalendarEventRepeatFrequency frequency,
        const QDate& untilDate
        );

    [[nodiscard]] Status updateRepeatSeriesFromDate(
        const CalendarEvent& originalEvent,
        const CalendarEvent& editedEvent
        );

    [[nodiscard]] Result<int> saveCalendarEvent(
        const CalendarEvent& event
        );

    [[nodiscard]] Result<QList<int>> saveCalendarEvents(
        const QList<CalendarEvent>& events
        );

    [[nodiscard]] Result<CalendarEventImportSummary> importCalendarEvents(
        const QList<CalendarEvent>& events,
        int parserSkippedCount
        );

    [[nodiscard]] Status deleteCalendarEvent(
        int eventId
        );

    [[nodiscard]] Status deleteCalendarEventsForRepeatSeriesFromDate(
        const QString& repeatSeriesId,
        const QDate& startDate
        );

    [[nodiscard]] Status deleteAllCalendarEvents();

private:
    [[nodiscard]] Status ensureEngineDatabase(
        const QString& operation
        );

    QString m_databasePath;
    bool m_compatibilityDatabaseWasOpen = true;
    std::unique_ptr<classmngr::engine::SqliteDatabase> m_engineDatabase;
    QString m_engineDatabasePath;
};
