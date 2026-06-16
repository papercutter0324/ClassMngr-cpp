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

    CalendarEvent getCalendarEvent(
        int eventId
        );

    int saveCalendarEvent(
        const CalendarEvent& event
        );

    void deleteCalendarEvent(
        int eventId
        );

private:
    QSqlDatabase& m_database;
};
