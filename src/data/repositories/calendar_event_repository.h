#pragma once

#include "domain/models/calendar_event.h"

#include <QList>
#include <QSqlDatabase>

class CalendarEventRepository
{
public:
    explicit CalendarEventRepository(
        QSqlDatabase& database
        );

    QList<CalendarEvent> loadCalendarEventsForDate(
        const QDate& date
        );

    QList<CalendarEvent> loadCalendarEventsInRange(
        const QDate& startDate,
        const QDate& endDate
        );

    QList<CalendarEvent> loadUpcomingCalendarEvents(
        const QDate& fromDate,
        int limit
        );

    CalendarEvent getCalendarEvent(
        int eventId
        );

    QList<CalendarEvent> loadCalendarEventsForRepeatSeriesFromDate(
        const QString& repeatSeriesId,
        const QDate& startDate
        );

    int saveCalendarEvent(
        const CalendarEvent& event
        );

    void deleteCalendarEvent(
        int eventId
        );

    void deleteCalendarEventsForRepeatSeriesFromDate(
        const QString& repeatSeriesId,
        const QDate& startDate
        );

    void deleteAllCalendarEvents();

private:
    QSqlDatabase& m_database;
};
