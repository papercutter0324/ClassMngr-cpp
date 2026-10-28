#include "calendar_event_model.h"

#include "calendar_event_cache.h"
#include "features/calendar/calendar_event_campus_filter.h"
#include "next/application/calendar_event_visibility_policy.h"
#include "next/application/calendar_event_projection.h"
#include "next/platform/calendar_event_time_status_adapter.h"

#include <QDateTime>
#include <QTime>
#include <QVariantMap>

#include <string>

namespace
{
using CalendarEventSummary =
    ClassMngr::Next::Application::CalendarEventSummary;
using CalendarEventVisibilityPolicy =
    ClassMngr::Next::Application::CalendarEventVisibilityPolicy;

QString projectionText(
    const std::string& value
    )
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
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

bool CalendarEventModel::isVisible(const CalendarEventSummary& event) const
{
    if (m_activeEventTypesSet
        && !m_activeEventTypes.contains(projectionText(event.eventType.value())))
    {
        return false;
    }

    return CalendarEventVisibilityPolicy::shouldShowEvent(
        event,
        m_hideStartOfTermEvents,
        [&]()
        {
            return CalendarEventCampusFilter::eventMatchesCampus(
                event,
                m_currentCampusCodes,
                m_allCampusCodes,
                m_showAllCampuses
                );
        }
        );
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
        if (!isVisible(event))
        {
            continue;
        }

        bool validId = false;
        const qint64 id = projectionText(event.id.value()).toLongLong(&validId);
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

            const QString rawStartTime =
                projectionText(*event.startTime);
            const QString rawEndTime =
                projectionText(*event.endTime);
            const QTime parsedStartTime =
                QTime::fromString(rawStartTime, QStringLiteral("HH:mm"));
            const QTime parsedEndTime =
                QTime::fromString(rawEndTime, QStringLiteral("HH:mm"));
            if (
                !parsedStartTime.isValid()
                || parsedStartTime.toString(QStringLiteral("HH:mm"))
                    != rawStartTime
                || !parsedEndTime.isValid()
                || parsedEndTime.toString(QStringLiteral("HH:mm"))
                    != rawEndTime
                )
            {
                continue;
            }

            startTime = parsedStartTime;
            endTime = parsedEndTime;
        }

        QVariantMap value;

        // QML numbers cannot represent every SQLite integer identity.
        value.insert(
            QStringLiteral("id"),
            QString::number(id)
            );
        value.insert(
            QStringLiteral("title"),
            projectionText(event.title)
            );
        value.insert(
            QStringLiteral("eventType"),
            projectionText(event.eventType.value())
            );
        value.insert(
            QStringLiteral("timeStatus"),
            ClassMngr::Next::Platform::calendarEventTimeStatusToLegacyText(
                event.timeStatus
                )
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

ClassMngr::Next::Application::CalendarEventProjection
CalendarEventModel::visibleEventsInRange(
    const QDate& startDate,
    const QDate& endDate
    ) const
{
    using namespace ClassMngr::Next::Application;
    if (!m_cache || !startDate.isValid() || !endDate.isValid()
        || endDate < startDate)
    {
        return {};
    }

    CalendarEventProjectionInput input;
    const CalendarEventProjection projection =
        m_cache->eventProjectionInRange(startDate, endDate);
    input.events.reserve(projection.events().size());
    for (const CalendarEventSummary& event : projection.events())
    {
        if (isVisible(event))
        {
            input.events.push_back(event);
        }
    }
    const auto filtered = CalendarEventProjection::create(std::move(input));
    return filtered ? filtered.value() : CalendarEventProjection{};
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

void CalendarEventModel::setActiveEventTypes(const QStringList& eventTypes)
{
    QStringList normalized;
    normalized.reserve(eventTypes.size());
    for (const QString& eventType : eventTypes)
    {
        const QString value = eventType.trimmed();
        if (!value.isEmpty() && !normalized.contains(value))
        {
            normalized.append(value);
        }
    }
    if (m_activeEventTypesSet && m_activeEventTypes == normalized)
    {
        return;
    }
    m_activeEventTypes = std::move(normalized);
    m_activeEventTypesSet = true;
    reload();
}

void CalendarEventModel::reload()
{
    ++m_revision;
    emit revisionChanged();
}
