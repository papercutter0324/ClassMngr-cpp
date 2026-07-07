#include "calendar_event_repository.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace
{
CalendarEvent eventFromQuery(
    const QSqlQuery& query
    )
{
    CalendarEvent event;

    event.id =
        query.value("id").toInt();
    event.title =
        query.value("title").toString();
    event.eventType =
        normalizedCalendarEventType(
            query.value("event_type").toString()
            );
    event.timeStatus =
        normalizedCalendarEventTimeStatus(
            query.value("time_status").toString()
            );
    event.repeatSeriesId =
        query.value("repeat_series_id").toString().trimmed();
    event.allDay =
        query.value("all_day").toBool();
    event.startDate =
        QDate::fromString(
            query.value("start_date").toString(),
            Qt::ISODate
            );
    event.startTime =
        QTime::fromString(
            query.value("start_time").toString(),
            QStringLiteral("HH:mm")
            );
    event.endDate =
        QDate::fromString(
            query.value("end_date").toString(),
            Qt::ISODate
            );
    event.endTime =
        QTime::fromString(
            query.value("end_time").toString(),
            QStringLiteral("HH:mm")
            );

    return event;
}
}

CalendarEventRepository::CalendarEventRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

QList<CalendarEvent> CalendarEventRepository::loadCalendarEventsForDate(
    const QDate& date
    )
{
    QList<CalendarEvent> events;

    if (!date.isValid())
    {
        return events;
    }

    QSqlQuery query(m_database);

    query.prepare(R"(
        SELECT
            id,
            title,
            event_type,
            time_status,
            repeat_series_id,
            all_day,
            start_date,
            start_time,
            end_date,
            end_time
        FROM calendar_events
        WHERE ? >= start_date
        AND ? <= end_date
        ORDER BY start_time, title
    )");

    const QString isoDate =
        date.toString(Qt::ISODate);

    query.addBindValue(isoDate);
    query.addBindValue(isoDate);

    if (!query.exec())
    {
        qWarning()
            << "Failed to load calendar events:"
            << query.lastError().text();

        return events;
    }

    while (query.next())
    {
        events.append(
            eventFromQuery(query)
            );
    }

    return events;
}

QList<CalendarEvent> CalendarEventRepository::loadCalendarEventsInRange(
    const QDate& startDate,
    const QDate& endDate
    )
{
    QList<CalendarEvent> events;

    if (
        !startDate.isValid()
        || !endDate.isValid()
        || endDate < startDate
        )
    {
        return events;
    }

    QSqlQuery query(m_database);

    query.prepare(R"(
        SELECT
            id,
            title,
            event_type,
            time_status,
            repeat_series_id,
            all_day,
            start_date,
            start_time,
            end_date,
            end_time
        FROM calendar_events
        WHERE end_date >= ?
        AND start_date <= ?
        ORDER BY start_date, start_time, title
    )");

    query.addBindValue(
        startDate.toString(Qt::ISODate)
        );
    query.addBindValue(
        endDate.toString(Qt::ISODate)
        );

    if (!query.exec())
    {
        qWarning()
            << "Failed to load calendar events in range:"
            << query.lastError().text();

        return events;
    }

    while (query.next())
    {
        events.append(
            eventFromQuery(query)
            );
    }

    return events;
}

QList<CalendarEvent> CalendarEventRepository::loadUpcomingCalendarEvents(
    const QDate& fromDate,
    int limit
    )
{
    QList<CalendarEvent> events;

    if (
        !fromDate.isValid()
        || limit <= 0
        )
    {
        return events;
    }

    QSqlQuery query(m_database);

    query.prepare(R"(
        SELECT
            id,
            title,
            event_type,
            time_status,
            repeat_series_id,
            all_day,
            start_date,
            start_time,
            end_date,
            end_time
        FROM calendar_events
        WHERE end_date >= ?
        ORDER BY start_date, start_time, title
        LIMIT ?
    )");

    query.addBindValue(
        fromDate.toString(Qt::ISODate)
        );
    query.addBindValue(limit);

    if (!query.exec())
    {
        qWarning()
            << "Failed to load upcoming calendar events:"
            << query.lastError().text();

        return events;
    }

    while (query.next())
    {
        events.append(
            eventFromQuery(query)
            );
    }

    return events;
}

CalendarEvent CalendarEventRepository::getCalendarEvent(
    int eventId
    )
{
    CalendarEvent event;

    if (eventId <= 0)
    {
        return event;
    }

    QSqlQuery query(m_database);

    query.prepare(R"(
        SELECT
            id,
            title,
            event_type,
            time_status,
            repeat_series_id,
            all_day,
            start_date,
            start_time,
            end_date,
            end_time
        FROM calendar_events
        WHERE id=?
    )");

    query.addBindValue(eventId);

    if (!query.exec())
    {
        qWarning()
            << "Failed to load calendar event:"
            << query.lastError().text();

        return event;
    }

    if (!query.next())
    {
        return event;
    }

    return eventFromQuery(query);
}

QList<CalendarEvent> CalendarEventRepository::loadCalendarEventsForRepeatSeriesFromDate(
    const QString& repeatSeriesId,
    const QDate& startDate
    )
{
    QList<CalendarEvent> events;

    const QString normalizedRepeatSeriesId =
        repeatSeriesId.trimmed();

    if (
        normalizedRepeatSeriesId.isEmpty()
        || !startDate.isValid()
        )
    {
        return events;
    }

    QSqlQuery query(m_database);

    query.prepare(R"(
        SELECT
            id,
            title,
            event_type,
            time_status,
            repeat_series_id,
            all_day,
            start_date,
            start_time,
            end_date,
            end_time
        FROM calendar_events
        WHERE repeat_series_id=?
        AND start_date >= ?
        ORDER BY start_date, start_time, title, id
    )");

    query.addBindValue(normalizedRepeatSeriesId);
    query.addBindValue(
        startDate.toString(Qt::ISODate)
        );

    if (!query.exec())
    {
        qWarning()
            << "Failed to load calendar repeat series events:"
            << query.lastError().text();

        return events;
    }

    while (query.next())
    {
        events.append(
            eventFromQuery(query)
            );
    }

    return events;
}

int CalendarEventRepository::saveCalendarEvent(
    const CalendarEvent& event
    )
{
    QSqlQuery query(m_database);
    const QString eventType =
        normalizedCalendarEventType(
            event.eventType
            );
    const QString timeStatus =
        event.allDay
            ? QStringLiteral("Timed")
            : normalizedCalendarEventTimeStatus(
                event.timeStatus
                );
    const QString repeatSeriesId =
        event.repeatSeriesId.trimmed();
    const QVariant repeatSeriesValue =
        repeatSeriesId.isEmpty()
            ? QVariant()
            : QVariant(repeatSeriesId);

    if (event.id > 0)
    {
        query.prepare(R"(
            UPDATE calendar_events
            SET
                title=?,
                event_type=?,
                time_status=?,
                repeat_series_id=?,
                all_day=?,
                start_date=?,
                start_time=?,
                end_date=?,
                end_time=?
            WHERE id=?
        )");

        query.addBindValue(event.title);
        query.addBindValue(eventType);
        query.addBindValue(timeStatus);
        query.addBindValue(repeatSeriesValue);
        query.addBindValue(event.allDay ? 1 : 0);
        query.addBindValue(event.startDate.toString(Qt::ISODate));
        query.addBindValue(event.startTime.toString(QStringLiteral("HH:mm")));
        query.addBindValue(event.endDate.toString(Qt::ISODate));
        query.addBindValue(event.endTime.toString(QStringLiteral("HH:mm")));
        query.addBindValue(event.id);

        if (!query.exec())
        {
            qWarning()
                << "Failed to update calendar event:"
                << query.lastError().text();

            return -1;
        }

        return event.id;
    }

    query.prepare(R"(
        INSERT INTO calendar_events (
            title,
            event_type,
            time_status,
            repeat_series_id,
            all_day,
            start_date,
            start_time,
            end_date,
            end_time
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");

    query.addBindValue(event.title);
    query.addBindValue(eventType);
    query.addBindValue(timeStatus);
    query.addBindValue(repeatSeriesValue);
    query.addBindValue(event.allDay ? 1 : 0);
    query.addBindValue(event.startDate.toString(Qt::ISODate));
    query.addBindValue(event.startTime.toString(QStringLiteral("HH:mm")));
    query.addBindValue(event.endDate.toString(Qt::ISODate));
    query.addBindValue(event.endTime.toString(QStringLiteral("HH:mm")));

    if (!query.exec())
    {
        qWarning()
            << "Failed to save calendar event:"
            << query.lastError().text();

        return -1;
    }

    return query.lastInsertId().toInt();
}

void CalendarEventRepository::deleteCalendarEvent(
    int eventId
    )
{
    if (eventId <= 0)
    {
        return;
    }

    QSqlQuery query(m_database);

    query.prepare(R"(
        DELETE FROM calendar_events
        WHERE id=?
    )");

    query.addBindValue(eventId);

    if (!query.exec())
    {
        qWarning()
            << "Failed to delete calendar event:"
            << query.lastError().text();
    }
}

void CalendarEventRepository::deleteCalendarEventsForRepeatSeriesFromDate(
    const QString& repeatSeriesId,
    const QDate& startDate
    )
{
    const QString normalizedRepeatSeriesId =
        repeatSeriesId.trimmed();

    if (
        normalizedRepeatSeriesId.isEmpty()
        || !startDate.isValid()
        )
    {
        return;
    }

    QSqlQuery query(m_database);

    query.prepare(R"(
        DELETE FROM calendar_events
        WHERE repeat_series_id=?
        AND start_date >= ?
    )");

    query.addBindValue(normalizedRepeatSeriesId);
    query.addBindValue(
        startDate.toString(Qt::ISODate)
        );

    if (!query.exec())
    {
        qWarning()
            << "Failed to delete calendar repeat series events:"
            << query.lastError().text();
    }
}

void CalendarEventRepository::deleteAllCalendarEvents()
{
    QSqlQuery query(m_database);

    if (!query.exec(QStringLiteral("DELETE FROM calendar_events")))
    {
        qWarning()
            << "Failed to delete calendar events:"
            << query.lastError().text();
    }
}
