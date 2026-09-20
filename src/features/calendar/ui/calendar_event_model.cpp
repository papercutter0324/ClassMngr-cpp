#include "calendar_event_model.h"

#include "calendar_event_cache.h"
#include "domain/models/calendar_event.h"
#include "features/calendar/calendar_event_campus_filter.h"

#include <QDateTime>
#include <QVariantMap>

#include <string>

namespace
{
using CalendarEventSummary =
    ClassMngr::Next::Application::CalendarEventSummary;

QString projectionText(
    const std::string& value
    )
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

bool isStartOfTermCalendarEvent(
    const CalendarEventSummary& event
    )
{
    const QString title =
        projectionText(event.title).simplified().toLower();

    return normalizedCalendarEventType(projectionText(event.eventType))
            == QStringLiteral("Other")
        && (
            title == QStringLiteral("new semester")
            || title == QStringLiteral("start of term")
            || title == QStringLiteral("term start")
            || title == QStringLiteral("term starts")
            );
}
}

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

    const auto projection =
        m_cache->eventProjectionForDate(date);

    for (const CalendarEventSummary& event : projection.events())
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

        bool validId = false;
        const int id = projectionText(event.id.value()).toInt(&validId);
        const QDate startDate = QDate::fromString(
            projectionText(event.startDate),
            Qt::ISODate
            );
        const QDate endDate = QDate::fromString(
            projectionText(event.endDate),
            Qt::ISODate
            );
        if (
            !validId
            || id <= 0
            || !startDate.isValid()
            || !endDate.isValid()
            || endDate < startDate
            )
        {
            continue;
        }

        QTime startTime;
        QTime endTime;
        if (event.startTime.has_value() || event.endTime.has_value())
        {
            if (!event.startTime.has_value() || !event.endTime.has_value())
            {
                continue;
            }

            startTime = QTime::fromString(
                projectionText(*event.startTime),
                QStringLiteral("HH:mm")
                );
            endTime = QTime::fromString(
                projectionText(*event.endTime),
                QStringLiteral("HH:mm")
                );
            if (!startTime.isValid() || !endTime.isValid())
            {
                continue;
            }
        }

        QVariantMap value;

        value.insert(
            QStringLiteral("id"),
            id
            );
        value.insert(
            QStringLiteral("title"),
            projectionText(event.title)
            );
        value.insert(
            QStringLiteral("eventType"),
            projectionText(event.eventType)
            );
        value.insert(
            QStringLiteral("timeStatus"),
            projectionText(event.timeStatus)
            );
        value.insert(
            QStringLiteral("allDay"),
            event.allDay
            );
        value.insert(
            QStringLiteral("start"),
            QDateTime(startDate, startTime)
            );
        value.insert(
            QStringLiteral("end"),
            QDateTime(endDate, endTime)
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
