#include "calendar_event_cache.h"

#include "classmngr/engine/calendar_event_service.h"
#include "classmngr/engine/open_database.h"

#include "core/memory_usage_diagnostics.h"

#include <QByteArray>
#include <QSet>
#include <QtConcurrentRun>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

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
    MemoryUsageDiagnostics::registerMemoryBreakdownProvider(this, this);

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
    m_dateIndexEntryCount = 0;
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

QList<MemoryBreakdownEntry> CalendarEventCache::memoryBreakdown() const
{
    const quint64 eventBytes = static_cast<quint64>(m_eventsById.size())
        * sizeof(CalendarEvent);
    const quint64 indexBytes = m_dateIndexEntryCount * sizeof(int);
    const QString rangeDetail = m_retainedRanges.isEmpty()
        ? QStringLiteral("none")
        : QStringLiteral("%1 to %2")
              .arg(
                  m_retainedRanges.first().startDate.toString(Qt::ISODate),
                  m_retainedRanges.last().endDate.toString(Qt::ISODate)
                  );

    return {
        {
            QStringLiteral("Calendar event cache"),
            QStringLiteral("Calendar"),
            eventBytes + indexBytes,
            static_cast<quint64>(m_eventsById.size()) + m_dateIndexEntryCount,
            QStringLiteral("events=%1; date indexes=%2; retained=%3; loaded ranges=%4; pending=%5")
                .arg(m_eventsById.size())
                .arg(m_dateIndexEntryCount)
                .arg(rangeDetail)
                .arg(m_loadedRanges.size())
                .arg(m_pendingRequests.size() + (m_activeRequest ? 1 : 0)),
            true
        }
    };
}

CalendarEventCache::LoadResult CalendarEventCache::load(
    const QString& databasePath,
    const Request& request
    )
{
    LoadResult result;
    result.request = request;

    const QByteArray utf8Path = databasePath.toUtf8();
    auto engineDatabase = classmngr::engine::OpenDatabase::execute(
        std::string_view(
            utf8Path.constData(),
            static_cast<std::size_t>(utf8Path.size())
            )
        );
    if (!engineDatabase)
    {
        const std::string& engineError = engineDatabase.error().message;
        result.error = QStringLiteral(
            "Unable to initialize calendar event cache database:\n%1\n\n%2"
            )
            .arg(
                databasePath,
                QString::fromUtf8(
                    engineError.data(),
                    static_cast<qsizetype>(engineError.size())
                    )
                );
        return result;
    }

    classmngr::engine::CalendarEventService service(**engineDatabase);
    if (request.kind == RequestKind::Range)
    {
        const auto events = service.loadInRange(
            calendar_event_detail::toEngineDate(request.startDate),
            calendar_event_detail::toEngineDate(request.endDate)
            );
        if (!events)
        {
            result.error = QString::fromUtf8(
                events.error().message.data(),
                static_cast<qsizetype>(events.error().message.size())
                );
            return result;
        }

        result.events.reserve(static_cast<qsizetype>(events->size()));
        for (const classmngr::engine::CalendarEvent& event : *events)
        {
            result.events.append(calendarEventFromEngine(event));
        }
    }
    else
    {
        const auto nextEventDate = service.findNextStartDate(
            calendar_event_detail::toEngineDate(request.startDate)
            );
        if (!nextEventDate)
        {
            result.error = QString::fromUtf8(
                nextEventDate.error().message.data(),
                static_cast<qsizetype>(nextEventDate.error().message.size())
                );
            return result;
        }

        if (nextEventDate->has_value())
        {
            result.nextEventDate =
                calendar_event_detail::fromEngineDate(**nextEventDate);
        }
    }

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

    m_activeRequestTiming = MemoryUsageDiagnostics::isEnabled();
    m_activeDiagnosticTaskId = 0;
    if (m_activeRequestTiming)
    {
        m_activeRequestTimer.start();
        m_activeDiagnosticTaskId =
            MemoryUsageDiagnostics::beginBackgroundTask(
                QStringLiteral("Calendar"),
                request.kind == RequestKind::Range
                    ? (request.priority == Priority::Foreground
                           ? QStringLiteral("foreground cache range")
                           : QStringLiteral("background cache range"))
                    : QStringLiteral("next-event month lookup")
                );
    }

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
    const qint64 elapsedMilliseconds = m_activeRequestTiming
        ? m_activeRequestTimer.elapsed()
        : -1;
    const quint64 diagnosticTaskId = m_activeDiagnosticTaskId;
    m_activeRequestTiming = false;
    m_activeDiagnosticTaskId = 0;

    m_activeRequest.reset();

    const bool acceptedResult =
        result.request.generation == m_generation
        && result.error.isEmpty();

    if (acceptedResult)
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

    if (elapsedMilliseconds >= 0)
    {
        MemoryUsageDiagnostics::recordTimedOperation(
            QStringLiteral("calendar-cache-fetch"),
            QStringLiteral("%1; events=%2; %3")
                .arg(
                    result.request.kind == RequestKind::Range
                        ? QStringLiteral("range")
                        : QStringLiteral("next-event month"),
                    QString::number(result.events.size()),
                    acceptedResult
                        ? QStringLiteral("completed")
                        : QStringLiteral("discarded-or-failed")
                    ),
            elapsedMilliseconds
            );
    }
    if (!acceptedResult)
    {
        MemoryUsageDiagnostics::recordEvent(
            result.error.isEmpty()
                ? QStringLiteral("calendar-cache-request-discarded")
                : QStringLiteral("calendar-cache-request-failed"),
            result.request.kind == RequestKind::Range
                ? QStringLiteral("range")
                : QStringLiteral("next-event month")
            );
    }
    MemoryUsageDiagnostics::finishBackgroundTask(
        diagnosticTaskId,
        !acceptedResult
        );

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
                    ++m_dateIndexEntryCount;
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

    const int initialEventCount = m_eventsById.size();
    const quint64 initialDateIndexCount = m_dateIndexEntryCount;

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
            m_dateIndexEntryCount -= static_cast<quint64>(iterator.value().size());
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

    const int evictedEvents = initialEventCount - m_eventsById.size();
    const quint64 evictedDateIndexes =
        initialDateIndexCount - m_dateIndexEntryCount;
    if (evictedEvents > 0 || evictedDateIndexes > 0)
    {
        MemoryUsageDiagnostics::recordEvent(
            QStringLiteral("calendar-cache-evicted"),
            QStringLiteral("events=%1; dateIndexes=%2")
                .arg(evictedEvents)
                .arg(evictedDateIndexes)
            );
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
        const qsizetype priorCount = iterator.value().size();
        iterator.value().removeAll(eventId);
        m_dateIndexEntryCount -= static_cast<quint64>(
            priorCount - iterator.value().size()
            );
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
