#include "calendar_event_model.h"

#include "data/data_service.h"
#include "domain/models/calendar_event.h"
#include "features/calendar/calendar_event_campus_filter.h"

#include <QDateTime>
#include <QVariantMap>

CalendarEventModel::CalendarEventModel(
    DataService* dataService,
    QObject* parent
    )
    : QObject(parent)
    , m_dataService(dataService)
{
}

int CalendarEventModel::revision() const
{
    return m_revision;
}

QVariantList CalendarEventModel::eventsForDate(
    int year,
    int month,
    int day
    ) const
{
    QVariantList values;

    if (
        !m_dataService
        || !m_dataService->isOpen()
        )
    {
        return values;
    }

    const QDate date(
        year,
        month,
        day
        );

    if (!date.isValid())
    {
        return values;
    }

    const QList<CalendarEvent> events =
        m_dataService->loadCalendarEventsForDate(
            date
            );

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
