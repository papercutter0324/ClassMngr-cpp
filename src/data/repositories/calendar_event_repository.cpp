#include "calendar_event_repository.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"

#include <QDebug>
#include <QObject>
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

QString eventIdentity(const CalendarEvent& event)
{
    if (event.id > 0)
    {
        return QObject::tr("calendar event id %1").arg(event.id);
    }

    return QObject::tr("calendar event '%1' on %2")
        .arg(event.title.trimmed(), event.startDate.toString(Qt::ISODate));
}
}

CalendarEventRepository::CalendarEventRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

Result<QList<CalendarEvent>> CalendarEventRepository::loadCalendarEventsForDate(
    const QDate& date
    )
{
    QList<CalendarEvent> events;

    if (!date.isValid())
    {
        return std::unexpected(
            QObject::tr("Loading calendar events failed: invalid date.")
            );
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

    const auto loaded = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading calendar events for date"),
        QObject::tr("date %1").arg(isoDate)
        );
    if (!loaded)
    {
        return std::unexpected(loaded.error().userMessage());
    }

    while (query.next())
    {
        events.append(
            eventFromQuery(query)
            );
    }

    return events;
}

Result<QList<CalendarEvent>> CalendarEventRepository::loadCalendarEventsInRange(
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
        return std::unexpected(
            QObject::tr("Loading calendar events failed: invalid date range.")
            );
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

    const auto loaded = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading calendar events in range"),
        QObject::tr("from %1 to %2")
            .arg(startDate.toString(Qt::ISODate), endDate.toString(Qt::ISODate))
        );
    if (!loaded)
    {
        return std::unexpected(loaded.error().userMessage());
    }

    while (query.next())
    {
        events.append(
            eventFromQuery(query)
            );
    }

    return events;
}

Result<QList<CalendarEvent>> CalendarEventRepository::loadUpcomingCalendarEvents(
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
        return std::unexpected(
            QObject::tr("Loading upcoming calendar events failed: invalid request.")
            );
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

    const auto loaded = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading upcoming calendar events"),
        QObject::tr("from %1, limit %2")
            .arg(fromDate.toString(Qt::ISODate))
            .arg(limit)
        );
    if (!loaded)
    {
        return std::unexpected(loaded.error().userMessage());
    }

    while (query.next())
    {
        events.append(
            eventFromQuery(query)
            );
    }

    return events;
}

Result<QDate> CalendarEventRepository::findNextCalendarEventStartDate(
    const QDate& fromDate
    )
{
    if (!fromDate.isValid())
    {
        return std::unexpected(
            QObject::tr("Finding next calendar event failed: invalid date.")
            );
    }

    QSqlQuery query(m_database);

    query.prepare(R"(
        SELECT MIN(start_date)
        FROM calendar_events
        WHERE start_date >= ?
    )");
    query.addBindValue(
        fromDate.toString(Qt::ISODate)
        );

    const auto loaded = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Finding next calendar event start date"),
        QObject::tr("from %1").arg(fromDate.toString(Qt::ISODate))
        );
    if (!loaded)
    {
        return std::unexpected(loaded.error().userMessage());
    }

    if (!query.next())
    {
        return {};
    }

    return QDate::fromString(
        query.value(0).toString(),
        Qt::ISODate
        );
}

Result<CalendarEvent> CalendarEventRepository::getCalendarEvent(
    int eventId
    )
{
    if (eventId <= 0)
    {
        return std::unexpected(
            QObject::tr("Loading calendar event failed: invalid event id %1.")
                .arg(eventId)
            );
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

    const auto loaded = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading calendar event"),
        QObject::tr("calendar event id %1").arg(eventId)
        );
    if (!loaded)
    {
        return std::unexpected(loaded.error().userMessage());
    }

    if (!query.next())
    {
        return std::unexpected(
            QObject::tr("Loading calendar event failed: no matching record exists for event id %1.")
                .arg(eventId)
            );
    }

    return eventFromQuery(query);
}

Result<QList<CalendarEvent>> CalendarEventRepository::loadCalendarEventsForRepeatSeriesFromDate(
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
        return std::unexpected(
            QObject::tr("Loading calendar repeat series failed: invalid series or start date.")
            );
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

    const auto loaded = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading calendar repeat series events"),
        QObject::tr("repeat series '%1' from %2")
            .arg(normalizedRepeatSeriesId, startDate.toString(Qt::ISODate))
        );
    if (!loaded)
    {
        return std::unexpected(loaded.error().userMessage());
    }

    while (query.next())
    {
        events.append(
            eventFromQuery(query)
            );
    }

    return events;
}

Result<int> CalendarEventRepository::saveCalendarEvent(
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

        const auto executed = SqlQueryUtils::executePrepared(
            query,
            QObject::tr("Updating calendar event"),
            eventIdentity(event)
            );
        if (!executed)
        {
            return std::unexpected(executed.error().userMessage());
        }
        if (query.numRowsAffected() == 0)
        {
            return std::unexpected(
                QObject::tr("Updating %1 failed: no matching record exists.")
                    .arg(eventIdentity(event))
                );
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

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Creating calendar event"),
        eventIdentity(event)
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    const int eventId = query.lastInsertId().toInt();
    if (eventId <= 0)
    {
        return std::unexpected(
            QObject::tr(
                "Creating %1 failed: the database did not return a valid "
                "record id."
                ).arg(eventIdentity(event))
            );
    }

    return eventId;
}

Result<QList<int>> CalendarEventRepository::saveCalendarEvents(
    const QList<CalendarEvent>& events
    )
{
    if (events.isEmpty())
    {
        return QList<int>{};
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr("Starting calendar event save transaction failed: %1")
                .arg(m_database.lastError().text())
            );
    }

    QList<int> eventIds;
    eventIds.reserve(events.size());
    for (const CalendarEvent& event : events)
    {
        const Result<int> saved = saveCalendarEvent(event);
        if (!saved)
        {
            return std::unexpected(saved.error());
        }
        eventIds.append(*saved);
    }

    if (!transaction.commit())
    {
        return std::unexpected(
            QObject::tr("Committing calendar event saves failed: %1")
                .arg(m_database.lastError().text())
            );
    }

    return eventIds;
}

Status CalendarEventRepository::deleteCalendarEvent(
    int eventId
    )
{
    if (eventId <= 0)
    {
        return std::unexpected(
            QObject::tr("Deleting calendar event failed: invalid event id %1.")
                .arg(eventId)
            );
    }

    QSqlQuery query(m_database);

    query.prepare(R"(
        DELETE FROM calendar_events
        WHERE id=?
    )");

    query.addBindValue(eventId);

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Deleting calendar event"),
        QObject::tr("calendar event id %1").arg(eventId)
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    return {};
}

Status CalendarEventRepository::deleteCalendarEventsForRepeatSeriesFromDate(
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
        return std::unexpected(
            QObject::tr(
                "Deleting calendar repeat series failed: invalid series or "
                "start date."
                )
            );
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

    const QString identity = QObject::tr("repeat series '%1' from %2")
        .arg(normalizedRepeatSeriesId, startDate.toString(Qt::ISODate));
    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Deleting calendar repeat series events"),
        identity
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    return {};
}

Status CalendarEventRepository::deleteAllCalendarEvents()
{
    QSqlQuery query(m_database);

    const auto executed = SqlQueryUtils::execute(
        query,
        QStringLiteral("DELETE FROM calendar_events"),
        QObject::tr("Deleting all calendar events")
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    return {};
}
