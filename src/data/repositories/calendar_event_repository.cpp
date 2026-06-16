#include "calendar_event_repository.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

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
        query.value("event_type").toString().trimmed().isEmpty()
            ? QStringLiteral("Other")
            : query.value("event_type").toString().trimmed();
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

int CalendarEventRepository::saveCalendarEvent(
    const CalendarEvent& event
    )
{
    QSqlQuery query(m_database);
    const QString eventType =
        event.eventType.trimmed().isEmpty()
            ? QStringLiteral("Other")
            : event.eventType.trimmed();

    if (event.id > 0)
    {
        query.prepare(R"(
            UPDATE calendar_events
            SET
                title=?,
                event_type=?,
                all_day=?,
                start_date=?,
                start_time=?,
                end_date=?,
                end_time=?
            WHERE id=?
        )");

        query.addBindValue(event.title);
        query.addBindValue(eventType);
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
            all_day,
            start_date,
            start_time,
            end_date,
            end_time
        )
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )");

    query.addBindValue(event.title);
    query.addBindValue(eventType);
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
