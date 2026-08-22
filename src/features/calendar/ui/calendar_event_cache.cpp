#include "calendar_event_cache.h"

#include "data/database/database_schema_manager.h"
#include "data/repositories/calendar_event_repository.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QUuid>
#include <QtConcurrentRun>

#include <algorithm>

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
    m_eventsByDate.clear();
    m_loadedRanges.clear();
    m_pendingRequests.clear();

    emit cacheChanged();
    emitLoadingChangedIfNeeded(previouslyLoading);
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
    return isRangeLoaded(date, date)
        ? m_eventsByDate.value(date)
        : QList<CalendarEvent>();
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

    for (const CalendarEvent& event : m_eventsById)
    {
        if (
            event.endDate >= startDate
            && event.startDate <= endDate
            )
        {
            events.append(event);
        }
    }

    std::sort(
        events.begin(),
        events.end(),
        [](const CalendarEvent& left, const CalendarEvent& right)
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
            insertEvents(
                result.events,
                result.request.startDate,
                result.request.endDate
                );

            markRangeLoaded(
                result.request.startDate,
                result.request.endDate
                );
            emit cacheChanged();
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
    const QDate& loadedStartDate,
    const QDate& loadedEndDate
    )
{
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

        m_eventsById.insert(event.id, event);

        for (
            QDate date = qMax(event.startDate, loadedStartDate);
            date <= qMin(event.endDate, loadedEndDate);
            date = date.addDays(1)
            )
        {
            QList<CalendarEvent>& dateEvents =
                m_eventsByDate[date];
            bool replaced = false;

            for (CalendarEvent& existing : dateEvents)
            {
                if (existing.id == event.id)
                {
                    existing = event;
                    replaced = true;
                    break;
                }
            }

            if (!replaced)
            {
                dateEvents.append(event);
            }

            std::sort(
                dateEvents.begin(),
                dateEvents.end(),
                [](const CalendarEvent& left, const CalendarEvent& right)
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
                );
        }
    }
}

void CalendarEventCache::markRangeLoaded(
    const QDate& startDate,
    const QDate& endDate
    )
{
    m_loadedRanges.append({startDate, endDate});

    std::sort(
        m_loadedRanges.begin(),
        m_loadedRanges.end(),
        [](const DateRange& left, const DateRange& right)
        {
            return left.startDate < right.startDate;
        }
        );

    QList<DateRange> merged;

    for (const DateRange& range : m_loadedRanges)
    {
        if (
            merged.isEmpty()
            || merged.last().startDate > range.endDate.addDays(1)
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

    m_loadedRanges = merged;
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

void CalendarEventCache::emitLoadingChangedIfNeeded(
    bool previouslyLoading
    )
{
    if (previouslyLoading != isLoading())
    {
        emit loadingChanged();
    }
}
