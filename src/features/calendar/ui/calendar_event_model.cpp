#include "calendar_event_model.h"

#include "calendar_event_cache.h"
#include "domain/models/calendar_event.h"
#include "features/calendar/calendar_event_campus_filter.h"

#include <QDateTime>
#include <QVariantMap>

CalendarEventModel::CalendarEventModel(
    CalendarEventCache* cache,
    QObject* parent
    )
    : QObject(parent)
    , m_cache(cache)
{
    if (!m_cache)
    {
        return;
    }

    connect(
        m_cache,
        &CalendarEventCache::cacheChanged,
        this,
        &CalendarEventModel::reload
        );
    connect(
        m_cache,
        &CalendarEventCache::loadingChanged,
        this,
        [this]()
        {
            emit loadingChanged();
            reload();
        }
        );
}

int CalendarEventModel::revision() const
{
    return m_revision;
}

bool CalendarEventModel::isLoading() const
{
    return m_cache && m_cache->isLoading();
}

QVariantList CalendarEventModel::eventsForDate(
    int year,
    int month,
    int day
    ) const
{
    QVariantList values;

    const QDate date(
        year,
        month,
        day
        );

    if (!date.isValid())
    {
        return values;
    }

    if (!m_cache)
    {
        return values;
    }

    const QList<CalendarEvent> events =
        m_cache->eventsForDate(date);

    for (const CalendarEvent& event : events)
    {
        if (
            m_hideStartOfTermEvents
            && isStartOfTermCalendarEvent(event)
            )
        {
            continue;
        }

        if (
            !CalendarEventCampusFilter::eventMatchesCampus(
                event,
                m_currentCampusCodes,
                m_allCampusCodes,
                m_showAllCampuses
                )
            )
        {
            continue;
        }

        QVariantMap value;

        value.insert(
            QStringLiteral("id"),
            event.id
            );
        value.insert(
            QStringLiteral("title"),
            event.title
            );
        value.insert(
            QStringLiteral("eventType"),
            event.eventType
            );
        value.insert(
            QStringLiteral("timeStatus"),
            event.timeStatus
            );
        value.insert(
            QStringLiteral("allDay"),
            event.allDay
            );
        value.insert(
            QStringLiteral("start"),
            QDateTime(event.startDate, event.startTime)
            );
        value.insert(
            QStringLiteral("end"),
            QDateTime(event.endDate, event.endTime)
            );

        values.append(value);
    }

    return values;
}

bool CalendarEventModel::isMonthLoaded(
    int year,
    int month
    ) const
{
    if (!m_cache)
    {
        return false;
    }

    const QDate firstOfMonth(year, month, 1);

    return firstOfMonth.isValid()
        && m_cache->isRangeLoaded(
            firstOfMonth,
            firstOfMonth.addMonths(1).addDays(-1)
            );
}

void CalendarEventModel::setCampusFilter(
    const QStringList& currentCampusCodes,
    const QStringList& allCampusCodes,
    bool showAllCampuses,
    bool hideStartOfTermEvents
    )
{
    if (
        m_currentCampusCodes == currentCampusCodes
        && m_allCampusCodes == allCampusCodes
        && m_showAllCampuses == showAllCampuses
        && m_hideStartOfTermEvents == hideStartOfTermEvents
        )
    {
        return;
    }

    m_currentCampusCodes =
        currentCampusCodes;
    m_allCampusCodes =
        allCampusCodes;
    m_showAllCampuses =
        showAllCampuses;
    m_hideStartOfTermEvents =
        hideStartOfTermEvents;

    reload();
}

void CalendarEventModel::reload()
{
    ++m_revision;
    emit revisionChanged();
}
