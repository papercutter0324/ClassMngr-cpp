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

    [[nodiscard]] Result<int> saveCalendarEvent(
        const CalendarEvent& event
        );

    [[nodiscard]] Result<QList<int>> saveCalendarEvents(
        const QList<CalendarEvent>& events
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

    QSqlDatabase& m_database;
    std::unique_ptr<classmngr::engine::SqliteDatabase> m_engineDatabase;
    QString m_engineDatabasePath;
};
