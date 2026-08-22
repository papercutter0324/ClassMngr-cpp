#include "calendar_event_cache.h"

#include "data/database/database_schema_manager.h"
#include "data/repositories/calendar_event_repository.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSet>
#include <QUuid>
#include <QtConcurrentRun>

#include <algorithm>

namespace
{
bool eventComesBefore(
    const CalendarEvent& left,
    const CalendarEvent& right
    )
{
    if (left.startDate != right.startDate)
    {
        return left.startDate < right.startDate;
    }

    if (left.startTime != right.startTime)
    {
        return left.startTime < right.startTime;
    }

    if (left.title != right.title)
    {
        return left.title < right.title;
    }

    return left.id < right.id;
}

bool eventComesBeforeOnDate(
    const CalendarEvent& left,
    const CalendarEvent& right
    )
{
    if (left.startTime != right.startTime)
    {
        return left.startTime < right.startTime;
    }

    if (left.title != right.title)
    {
        return left.title < right.title;
    }

    return left.id < right.id;
}

QList<CalendarEventCache::DateRange> normalizedRanges(
    QList<CalendarEventCache::DateRange> ranges
    )
{
    ranges.erase(
        std::remove_if(
            ranges.begin(),
            ranges.end(),
            [](const CalendarEventCache::DateRange& range)
            {
                return !range.startDate.isValid()
                    || !range.endDate.isValid()
                    || range.endDate < range.startDate;
            }
            ),
        ranges.end()
        );
    std::sort(
        ranges.begin(),
        ranges.end(),
        [](const CalendarEventCache::DateRange& left,
           const CalendarEventCache::DateRange& right)
        {
            return left.startDate < right.startDate;
        }
        );

    QList<CalendarEventCache::DateRange> merged;
    for (const CalendarEventCache::DateRange& range : ranges)
    {
        if (
            merged.isEmpty()
            || range.startDate > merged.last().endDate.addDays(1)
            )
        {
            merged.append(range);
            continue;
        }

        merged.last().endDate = qMax(
            merged.last().endDate,
            range.endDate
            );
    }

    return merged;
}
}

CalendarEventCache::CalendarEventCache(
    QObject* parent
    )
    : QObject(parent)
{
    connect(
        &m_watcher,
        &QFutureWatcher<LoadResult>::finished,
        this,
        &CalendarEventCache::finishActiveRequest
        );
}

void CalendarEventCache::setDatabasePath(
    const QString& databasePath
    )
{
    if (m_databasePath == databasePath)
    {
        return;
    }

    m_databasePath = databasePath;
    invalidate();
}

QString CalendarEventCache::databasePath() const
{
    return m_databasePath;
}

void CalendarEventCache::invalidate()
{
    const bool previouslyLoading = isLoading();

    ++m_generation;
    m_eventsById.clear();
    m_eventIdsByDate.clear();
    m_loadedRanges.clear();
    m_pendingRequests.clear();

    emit cacheChanged();
    emitLoadingChangedIfNeeded(previouslyLoading);
}

void CalendarEventCache::setRetainedRanges(
    const QList<DateRange>& ranges
    )
{
    const QList<DateRange> normalized =
        normalizedRanges(ranges);

    if (m_retentionEnabled && m_retainedRanges == normalized)
    {
        return;
    }

    m_retainedRanges = normalized;
    m_retentionEnabled = true;
    pruneToRetainedRanges();
    emit cacheChanged();
}

void CalendarEventCache::requestRange(
    const QDate& startDate,
    const QDate& endDate,
    Priority priority
    )
{
    if (
        m_databasePath.trimmed().isEmpty()
        || !startDate.isValid()
        || !endDate.isValid()
        || endDate < startDate
        || isRangeLoaded(startDate, endDate)
        || hasPendingRange(startDate, endDate)
        )
    {
        return;
    }

    enqueue(
        {
            RequestKind::Range,
            startDate,
            endDate,
            m_generation
        },
        priority
        );
}

void CalendarEventCache::requestNextEventMonth(
    const QDate& afterDate,
    Priority priority
    )
{
    if (
        m_databasePath.trimmed().isEmpty()
        || !afterDate.isValid()
        )
    {
        return;
    }

    for (const Request& request : m_pendingRequests)
    {
        if (
            request.kind == RequestKind::NextEventMonth
            && request.startDate == afterDate
            )
        {
            return;
        }
    }

    if (
        m_activeRequest
        && m_activeRequest->kind == RequestKind::NextEventMonth
        && m_activeRequest->startDate == afterDate
        )
    {
        return;
    }

    enqueue(
        {
            RequestKind::NextEventMonth,
            afterDate,
            {},
            m_generation
        },
        priority
        );
}

QList<CalendarEvent> CalendarEventCache::eventsForDate(
    const QDate& date
    ) const
{
    QList<CalendarEvent> events;
    if (!isRangeLoaded(date, date))
    {
        return events;
    }

    const auto eventIds = m_eventIdsByDate.constFind(date);
    if (eventIds == m_eventIdsByDate.cend())
    {
        return events;
    }

    events.reserve(eventIds->size());
    for (const int eventId : *eventIds)
    {
        const auto event = m_eventsById.constFind(eventId);
        if (event != m_eventsById.cend())
        {
            events.append(*event);
        }
    }

    return events;
}

QList<CalendarEvent> CalendarEventCache::eventsInRange(
    const QDate& startDate,
    const QDate& endDate
    ) const
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

    QSet<int> eventIds;
    for (
        QDate date = startDate;
        date <= endDate;
        date = date.addDays(1)
        )
    {
        const auto dateEventIds =
            m_eventIdsByDate.constFind(date);
        if (dateEventIds == m_eventIdsByDate.cend())
        {
            continue;
        }

        for (const int eventId : *dateEventIds)
        {
            eventIds.insert(eventId);
        }
    }

    events.reserve(eventIds.size());
    for (const int eventId : eventIds)
    {
        const auto event = m_eventsById.constFind(eventId);
        if (event != m_eventsById.cend())
        {
            events.append(*event);
        }
    }

    std::sort(
        events.begin(),
        events.end(),
        eventComesBefore
        );

    return events;
}

bool CalendarEventCache::isRangeLoaded(
    const QDate& startDate,
    const QDate& endDate
    ) const
{
    if (
        !startDate.isValid()
        || !endDate.isValid()
        || endDate < startDate
        )
    {
        return false;
    }

    for (const DateRange& range : m_loadedRanges)
    {
        if (
            range.startDate <= startDate
            && range.endDate >= endDate
            )
        {
            return true;
        }
    }

    return false;
}

bool CalendarEventCache::isLoading() const
{
    return m_activeRequest.has_value()
        || !m_pendingRequests.isEmpty();
}

int CalendarEventCache::eventCount() const
{
    return m_eventsById.size();
}

int CalendarEventCache::dateBucketCount() const
{
    return m_eventIdsByDate.size();
}

CalendarEventCache::LoadResult CalendarEventCache::load(
    const QString& databasePath,
    const Request& request
    )
{
    LoadResult result;
    result.request = request;

    const QString connectionName =
        QStringLiteral("calendar-event-cache-%1").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            );

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(databasePath);

        if (!database.open())
        {
            result.error = database.lastError().text();
        }
        else if (const Status foreignKeyStatus =
                     DatabaseSchemaManager::enableForeignKeyEnforcement(
                         database
                         );
                 !foreignKeyStatus)
        {
            result.error = foreignKeyStatus.error();
            database.close();
        }
        else
        {
            CalendarEventRepository repository(database);

            if (request.kind == RequestKind::Range)
            {
                const Result<QList<CalendarEvent>> events =
                    repository.loadCalendarEventsInRange(
                        request.startDate,
                        request.endDate
                        );
                if (events)
                {
                    result.events = *events;
                }
                else
                {
                    result.error = events.error();
                }
            }
            else
            {
                const Result<QDate> nextEventDate =
                    repository.findNextCalendarEventStartDate(
                        request.startDate
                        );
                if (nextEventDate)
                {
                    result.nextEventDate = *nextEventDate;
                }
                else
                {
                    result.error = nextEventDate.error();
                }
            }

            database.close();
        }
    }

    QSqlDatabase::removeDatabase(connectionName);
    return result;
}

void CalendarEventCache::enqueue(
    const Request& request,
    Priority priority
    )
{
    const bool previouslyLoading = isLoading();

    if (priority == Priority::Foreground)
    {
        Request queuedRequest = request;
        queuedRequest.priority = priority;
        auto insertAt = m_pendingRequests.cend();

        for (
            auto iterator = m_pendingRequests.cbegin();
            iterator != m_pendingRequests.cend();
            ++iterator
            )
        {
            if (iterator->priority == Priority::Background)
            {
                insertAt = iterator;
                break;
            }
        }

        m_pendingRequests.insert(insertAt, queuedRequest);
    }
    else
    {
        Request queuedRequest = request;
        queuedRequest.priority = priority;
        m_pendingRequests.append(queuedRequest);
    }

    startNextRequest();
    emitLoadingChangedIfNeeded(previouslyLoading);
}

void CalendarEventCache::startNextRequest()
{
    if (
        m_activeRequest
        || m_pendingRequests.isEmpty()
        )
    {
        return;
    }

    m_activeRequest = m_pendingRequests.takeFirst();
    const Request request = *m_activeRequest;
    const QString databasePath = m_databasePath;

    m_watcher.setFuture(
        QtConcurrent::run(
            [databasePath, request]()
            {
                return load(databasePath, request);
            }
            )
        );
}

void CalendarEventCache::finishActiveRequest()
{
    const bool previouslyLoading = isLoading();
    const LoadResult result = m_watcher.result();

    m_activeRequest.reset();

    if (
        result.request.generation == m_generation
        && result.error.isEmpty()
        )
    {
        if (result.request.kind == RequestKind::Range)
        {
            const QList<DateRange> loadedRanges =
                retainedRangesWithin(
                    {
                        result.request.startDate,
                        result.request.endDate
                    }
                    );
            if (!loadedRanges.isEmpty())
            {
                insertEvents(
                    result.events,
                    loadedRanges
                    );

                for (const DateRange& range : loadedRanges)
                {
                    markRangeLoaded(
                        range.startDate,
                        range.endDate
                        );
                }
                emit cacheChanged();
            }
        }
        else
        {
            emit nextEventMonthFound(result.nextEventDate);
        }
    }

    startNextRequest();
    emitLoadingChangedIfNeeded(previouslyLoading);
}

void CalendarEventCache::insertEvents(
    const QList<CalendarEvent>& events,
    const QList<DateRange>& loadedRanges
    )
{
    QList<DateRange> indexedRanges =
        m_loadedRanges;
    indexedRanges.append(loadedRanges);
    indexedRanges = normalizedRanges(indexedRanges);

    for (const CalendarEvent& event : events)
    {
        if (
            event.id <= 0
            || !event.startDate.isValid()
            || !event.endDate.isValid()
            || event.endDate < event.startDate
            )
        {
            continue;
        }

        removeEventMemberships(event.id);
        m_eventsById.insert(event.id, event);

        for (const DateRange& range : indexedRanges)
        {
            for (
                QDate date = qMax(event.startDate, range.startDate);
                date <= qMin(event.endDate, range.endDate);
                date = date.addDays(1)
                )
            {
                QList<int>& eventIds =
                    m_eventIdsByDate[date];
                if (!eventIds.contains(event.id))
                {
                    eventIds.append(event.id);
                }

                std::sort(
                    eventIds.begin(),
                    eventIds.end(),
                    [this](int leftId, int rightId)
                    {
                        return eventComesBeforeOnDate(
                            m_eventsById.value(leftId),
                            m_eventsById.value(rightId)
                            );
                    }
                    );
            }
        }
    }
}

void CalendarEventCache::markRangeLoaded(
    const QDate& startDate,
    const QDate& endDate
    )
{
    m_loadedRanges.append({startDate, endDate});
    m_loadedRanges = normalizedRanges(m_loadedRanges);
}

bool CalendarEventCache::hasPendingRange(
    const QDate& startDate,
    const QDate& endDate
    ) const
{
    const auto coversRange =
        [startDate, endDate](const Request& request)
        {
            return request.kind == RequestKind::Range
                && request.startDate <= startDate
                && request.endDate >= endDate;
        };

    if (m_activeRequest && coversRange(*m_activeRequest))
    {
        return true;
    }

    for (const Request& request : m_pendingRequests)
    {
        if (coversRange(request))
        {
            return true;
        }
    }

    return false;
}

QList<CalendarEventCache::DateRange>
CalendarEventCache::retainedRangesWithin(
    const DateRange& range
    ) const
{
    if (!m_retentionEnabled)
    {
        return {range};
    }

    QList<DateRange> intersections;
    for (const DateRange& retainedRange : m_retainedRanges)
    {
        const QDate startDate = qMax(
            range.startDate,
            retainedRange.startDate
            );
        const QDate endDate = qMin(
            range.endDate,
            retainedRange.endDate
            );
        if (startDate <= endDate)
        {
            intersections.append({startDate, endDate});
        }
    }

    return intersections;
}

bool CalendarEventCache::isDateRetained(
    const QDate& date
    ) const
{
    if (!m_retentionEnabled)
    {
        return true;
    }

    for (const DateRange& range : m_retainedRanges)
    {
        if (range.startDate <= date && date <= range.endDate)
        {
            return true;
        }
    }

    return false;
}

void CalendarEventCache::pruneToRetainedRanges()
{
    if (!m_retentionEnabled)
    {
        return;
    }

    QList<DateRange> retainedLoadedRanges;
    for (const DateRange& loadedRange : m_loadedRanges)
    {
        retainedLoadedRanges.append(
            retainedRangesWithin(loadedRange)
            );
    }
    m_loadedRanges = normalizedRanges(retainedLoadedRanges);

    QSet<int> referencedEventIds;
    for (
        auto iterator = m_eventIdsByDate.begin();
        iterator != m_eventIdsByDate.end();
        )
    {
        if (!isDateRetained(iterator.key()))
        {
            iterator = m_eventIdsByDate.erase(iterator);
            continue;
        }

        for (const int eventId : iterator.value())
        {
            referencedEventIds.insert(eventId);
        }
        ++iterator;
    }

    for (
        auto iterator = m_eventsById.begin();
        iterator != m_eventsById.end();
        )
    {
        if (!referencedEventIds.contains(iterator.key()))
        {
            iterator = m_eventsById.erase(iterator);
            continue;
        }
        ++iterator;
    }
}

void CalendarEventCache::removeEventMemberships(
    int eventId
    )
{
    for (
        auto iterator = m_eventIdsByDate.begin();
        iterator != m_eventIdsByDate.end();
        )
    {
        iterator.value().removeAll(eventId);
        if (iterator.value().isEmpty())
        {
            iterator = m_eventIdsByDate.erase(iterator);
            continue;
        }
        ++iterator;
    }
}

void CalendarEventCache::emitLoadingChangedIfNeeded(
    bool previouslyLoading
    )
{
    if (previouslyLoading != isLoading())
    {
        emit loadingChanged();
    }
}
