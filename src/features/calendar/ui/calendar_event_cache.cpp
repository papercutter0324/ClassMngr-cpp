#include "calendar_event_cache.h"

#include "domain/rules/schedule_value_parser.h"
#include "features/calendar/calendar_event_projection_query.h"
#include "next/platform/calendar_event_time_status_adapter.h"

#include <QSet>
#include <QtConcurrentRun>

#include <algorithm>
#include <optional>
#include <string>

namespace
{
using CalendarEventProjection =
    ClassMngr::Next::Application::CalendarEventProjection;
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

std::optional<qint64> legacyIdFromProjection(
    const CalendarEventSummary& summary
    )
{
    bool validId = false;
    const qint64 id = projectionText(summary.id.value()).toLongLong(&validId);
    if (!validId || id <= 0)
    {
        return std::nullopt;
    }

    return id;
}

bool summaryTimeComesBefore(
    const CalendarEventSummary& left,
    const CalendarEventSummary& right
    )
{
    if (left.startTime.has_value() != right.startTime.has_value())
    {
        return !left.startTime.has_value();
    }

    if (left.startTime && right.startTime && *left.startTime != *right.startTime)
    {
        return *left.startTime < *right.startTime;
    }

    return false;
}

bool summaryComesBefore(
    const CalendarEventSummary& left,
    const CalendarEventSummary& right
    )
{
    if (left.startDate != right.startDate)
    {
        return left.startDate < right.startDate;
    }

    if (summaryTimeComesBefore(left, right))
    {
        return true;
    }
    if (summaryTimeComesBefore(right, left))
    {
        return false;
    }

    if (left.title != right.title)
    {
        return left.title < right.title;
    }

    const auto leftId = legacyIdFromProjection(left);
    const auto rightId = legacyIdFromProjection(right);
    if (leftId && rightId && *leftId != *rightId)
    {
        return *leftId < *rightId;
    }

    return left.id.value() < right.id.value();
}

bool summaryComesBeforeOnDate(
    const CalendarEventSummary& left,
    const CalendarEventSummary& right
    )
{
    if (summaryTimeComesBefore(left, right))
    {
        return true;
    }
    if (summaryTimeComesBefore(right, left))
    {
        return false;
    }

    if (left.title != right.title)
    {
        return left.title < right.title;
    }

    const auto leftId = legacyIdFromProjection(left);
    const auto rightId = legacyIdFromProjection(right);
    if (leftId && rightId && *leftId != *rightId)
    {
        return *leftId < *rightId;
    }

    return left.id.value() < right.id.value();
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

CalendarEventProjection projectionFromSummaries(
    std::vector<CalendarEventSummary> summaries
    )
{
    ClassMngr::Next::Application::CalendarEventProjectionInput input;
    input.events = std::move(summaries);
    const auto projection = CalendarEventProjection::create(std::move(input));
    return projection ? projection.value() : CalendarEventProjection{};
}

std::optional<CalendarEvent> legacyEventFromProjection(
    const CalendarEventSummary& summary
    )
{
    const auto id = legacyIdFromProjection(summary);
    const QDate startDate =
        QDate::fromString(projectionText(summary.startDate), Qt::ISODate);
    const QDate endDate =
        QDate::fromString(projectionText(summary.endDate), Qt::ISODate);
    if (
        !id
        || !startDate.isValid()
        || !endDate.isValid()
        || endDate < startDate
        )
    {
        return std::nullopt;
    }

    CalendarEvent event;
    event.id = *id;
    event.title = projectionText(summary.title);
    event.eventType = projectionText(summary.eventType);
    event.timeStatus =
        ClassMngr::Next::Platform::calendarEventTimeStatusToLegacyText(
            summary.timeStatus
            );
    event.repeatSeriesId = summary.repeatSeriesId
        ? projectionText(summary.repeatSeriesId->value())
        : QString();
    event.allDay = summary.allDay;
    event.startDate = startDate;
    event.endDate = endDate;

    if (!summary.allDay && summary.startTime && summary.endTime)
    {
        const QString rawStartTime =
            projectionText(*summary.startTime);
        const QString rawEndTime =
            projectionText(*summary.endTime);
        const auto parsedStartTime =
            ScheduleValueParser::parseTime(rawStartTime);
        const auto parsedEndTime =
            ScheduleValueParser::parseTime(rawEndTime);
        if (
            !parsedStartTime
            || parsedStartTime->text != rawStartTime
            || !parsedEndTime
            || parsedEndTime->text != rawEndTime
            )
        {
            return std::nullopt;
        }

        event.startTime = parsedStartTime->value;
        event.endTime = parsedEndTime->value;
    }

    return event;
}

QList<CalendarEvent> legacyEventsFromProjection(
    const CalendarEventProjection& projection
    )
{
    QList<CalendarEvent> events;
    events.reserve(
        static_cast<qsizetype>(projection.eventCount())
        );

    for (const CalendarEventSummary& summary : projection.events())
    {
        const auto event = legacyEventFromProjection(summary);
        if (event)
        {
            events.append(*event);
        }
    }

    return events;
}

QString projectionErrorText(
    const ClassMngr::Next::Domain::OperationError& error
    )
{
    const QString message =
        QString::fromUtf8(
            error.message.data(),
            static_cast<qsizetype>(error.message.size())
            );
    return message.isEmpty()
        ? QStringLiteral("Calendar event query failed.")
        : message;
}
}

CalendarEventCache::CalendarEventCache(
    QObject* parent
    )
    : CalendarEventCache(
          std::make_shared<CalendarEventProjectionQueryFactory>(),
          parent
          )
{
}

CalendarEventCache::CalendarEventCache(
    std::shared_ptr<
        const ClassMngr::Next::Application::CalendarEventQueryPortFactory
        > queryFactory,
    QObject* parent
    )
    : QObject(parent),
      m_queryFactory(std::move(queryFactory))
{
    if (!m_queryFactory)
    {
        m_queryFactory =
            std::make_shared<CalendarEventProjectionQueryFactory>();
    }

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
    m_rangeFailures.clear();
    m_nextEventFailure.reset();
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

    const DateRange requestedRange{startDate, endDate};
    const auto previousFailure = std::find_if(
        m_rangeFailures.begin(),
        m_rangeFailures.end(),
        [&requestedRange](const RangeFailure& failure)
        {
            return failure.range == requestedRange;
        }
        );
    if (previousFailure != m_rangeFailures.end())
    {
        m_rangeFailures.erase(previousFailure);
        emit cacheChanged();
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
            && request.generation == m_generation
            && request.startDate == afterDate
            )
        {
            return;
        }
    }

    if (
        m_activeRequest
        && m_activeRequest->kind == RequestKind::NextEventMonth
        && m_activeRequest->generation == m_generation
        && m_activeRequest->startDate == afterDate
        )
    {
        return;
    }

    if (m_nextEventFailure)
    {
        m_nextEventFailure.reset();
        emit cacheChanged();
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

CalendarEventProjection CalendarEventCache::eventProjectionForDate(
    const QDate& date
    ) const
{
    if (!date.isValid() || !isRangeLoaded(date, date))
    {
        return {};
    }

    const auto eventIds = m_eventIdsByDate.constFind(date);
    if (eventIds == m_eventIdsByDate.cend())
    {
        return {};
    }

    std::vector<CalendarEventSummary> summaries;
    summaries.reserve(static_cast<std::size_t>(eventIds->size()));
    for (const qint64 eventId : *eventIds)
    {
        const auto event = m_eventsById.constFind(eventId);
        if (event != m_eventsById.cend())
        {
            summaries.push_back(event.value());
        }
    }

    return projectionFromSummaries(std::move(summaries));
}

CalendarEventProjection CalendarEventCache::eventProjectionInRange(
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
        return {};
    }

    QSet<qint64> eventIds;
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

        for (const qint64 eventId : *dateEventIds)
        {
            eventIds.insert(eventId);
        }
    }

    std::vector<CalendarEventSummary> summaries;
    summaries.reserve(static_cast<std::size_t>(eventIds.size()));
    for (const qint64 eventId : eventIds)
    {
        const auto event = m_eventsById.constFind(eventId);
        if (event != m_eventsById.cend())
        {
            summaries.push_back(event.value());
        }
    }

    std::sort(summaries.begin(), summaries.end(), summaryComesBefore);

    return projectionFromSummaries(std::move(summaries));
}

QList<CalendarEvent> CalendarEventCache::eventsForDate(
    const QDate& date
    ) const
{
    return legacyEventsFromProjection(
        eventProjectionForDate(date)
        );
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

    QSet<qint64> eventIds;
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

        for (const qint64 eventId : *dateEventIds)
        {
            eventIds.insert(eventId);
        }
    }

    std::vector<CalendarEventSummary> summaries;
    summaries.reserve(static_cast<std::size_t>(eventIds.size()));
    for (const qint64 eventId : eventIds)
    {
        const auto event = m_eventsById.constFind(eventId);
        if (event != m_eventsById.cend())
        {
            summaries.push_back(event.value());
        }
    }

    std::sort(summaries.begin(), summaries.end(), summaryComesBefore);

    return legacyEventsFromProjection(
        projectionFromSummaries(std::move(summaries))
        );
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

QString CalendarEventCache::rangeError(
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
        return {};
    }

    const DateRange requestedRange{startDate, endDate};
    const auto failure = std::find_if(
        m_rangeFailures.cbegin(),
        m_rangeFailures.cend(),
        [&requestedRange](const RangeFailure& candidate)
        {
            return candidate.range == requestedRange;
        }
        );
    return failure == m_rangeFailures.cend()
        ? QString()
        : failure->error;
}

QString CalendarEventCache::nextEventError(
    const QDate& afterDate
    ) const
{
    if (
        !afterDate.isValid()
        || !m_nextEventFailure
        || m_nextEventFailure->afterDate != afterDate
        )
    {
        return {};
    }

    return m_nextEventFailure->error;
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

int CalendarEventCache::loadedRangeCount() const
{
    return m_loadedRanges.size();
}

int CalendarEventCache::retainedRangeCount() const
{
    return m_retainedRanges.size();
}

CalendarEventCache::LoadResult CalendarEventCache::load(
    const std::shared_ptr<
        const ClassMngr::Next::Application::CalendarEventQueryPortFactory
        >& queryFactory,
    const QString& databasePath,
    const Request& request
    )
{
    LoadResult result;
    result.request = request;

    if (!queryFactory)
    {
        result.error = QStringLiteral(
            "Calendar event query factory is unavailable."
            );
        return result;
    }

    const std::unique_ptr<
        ClassMngr::Next::Application::CalendarEventQueryPort
        > query = queryFactory->create();
    if (!query)
    {
        result.error = QStringLiteral(
            "Calendar event query port is unavailable."
            );
        return result;
    }

    const ClassMngr::Next::Application::CalendarEventDatabasePath
        databasePathValue(
            databasePath.toUtf8().toStdString()
        );

    if (request.kind == RequestKind::Range)
    {
        const auto projection = query->loadRange(
            {
                databasePathValue,
                ClassMngr::Next::Application::CalendarEventDate(
                    request.startDate.toString(Qt::ISODate)
                        .toUtf8()
                        .toStdString()
                    ),
                ClassMngr::Next::Application::CalendarEventDate(
                    request.endDate.toString(Qt::ISODate)
                        .toUtf8()
                        .toStdString()
                    )
            }
            );
        if (projection)
        {
            result.projection = projection.value();
        }
        else
        {
            result.error = projectionErrorText(projection.error());
        }
    }
    else
    {
        const auto nextEventDate = query->findNextEventDate(
            {
                databasePathValue,
                ClassMngr::Next::Application::CalendarEventDate(
                    request.startDate.toString(Qt::ISODate)
                        .toUtf8()
                        .toStdString()
                    )
            }
            );
        if (nextEventDate)
        {
            if (nextEventDate.value().isValid())
            {
                result.nextEventDate = QDate::fromString(
                    QString::fromUtf8(
                        nextEventDate.value().value().data(),
                        static_cast<qsizetype>(
                            nextEventDate.value().value().size()
                            )
                        ),
                    Qt::ISODate
                    );
            }
        }
        else
        {
            result.error = projectionErrorText(nextEventDate.error());
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
    const auto queryFactory = m_queryFactory;

    m_watcher.setFuture(
        QtConcurrent::run(
            [queryFactory, databasePath, request]()
            {
                return load(queryFactory, databasePath, request);
            }
            )
        );
}

void CalendarEventCache::finishActiveRequest()
{
    const bool previouslyLoading = isLoading();
    const LoadResult result = m_watcher.result();

    m_activeRequest.reset();

    if (result.request.generation == m_generation)
    {
        if (!result.error.isEmpty())
        {
            if (result.request.kind == RequestKind::Range)
            {
                const DateRange failedRange{
                    result.request.startDate,
                    result.request.endDate
                };
                if (!retainedRangesWithin(failedRange).isEmpty())
                {
                    const auto previousFailure = std::find_if(
                        m_rangeFailures.begin(),
                        m_rangeFailures.end(),
                        [&failedRange](const RangeFailure& failure)
                        {
                            return failure.range == failedRange;
                        }
                        );
                    if (previousFailure == m_rangeFailures.end())
                    {
                        m_rangeFailures.append({failedRange, result.error});
                    }
                    else
                    {
                        previousFailure->error = result.error;
                    }
                    emit cacheChanged();
                }
            }
            else
            {
                m_nextEventFailure = {
                    result.request.startDate,
                    result.error
                };
                emit cacheChanged();
                emit nextEventMonthFailed(result.request.startDate);
            }
        }
        else if (result.request.kind == RequestKind::Range)
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
                    result.projection.events(),
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
            m_nextEventFailure.reset();
            emit nextEventMonthFound(result.nextEventDate);
        }
    }

    startNextRequest();
    emitLoadingChangedIfNeeded(previouslyLoading);
}

void CalendarEventCache::insertEvents(
    const std::vector<CalendarEventSummary>& events,
    const QList<DateRange>& loadedRanges
    )
{
    QList<DateRange> indexedRanges =
        m_loadedRanges;
    indexedRanges.append(loadedRanges);
    indexedRanges = normalizedRanges(indexedRanges);

    for (const CalendarEventSummary& event : events)
    {
        const auto eventId = legacyIdFromProjection(event);
        const QDate startDate = QDate::fromString(
            projectionText(event.startDate),
            Qt::ISODate
            );
        const QDate endDate = QDate::fromString(
            projectionText(event.endDate),
            Qt::ISODate
            );
        if (
            !eventId
            || !startDate.isValid()
            || !endDate.isValid()
            || endDate < startDate
            )
        {
            continue;
        }

        removeEventMemberships(*eventId);
        m_eventsById.insert(*eventId, event);

        for (const DateRange& range : indexedRanges)
        {
            for (
                QDate date = qMax(startDate, range.startDate);
                date <= qMin(endDate, range.endDate);
                date = date.addDays(1)
                )
            {
                QList<qint64>& eventIds =
                    m_eventIdsByDate[date];
                if (!eventIds.contains(*eventId))
                {
                    eventIds.append(*eventId);
                    ++m_dateIndexEntryCount;
                }

                std::sort(
                    eventIds.begin(),
                    eventIds.end(),
                    [this](qint64 leftId, qint64 rightId)
                    {
                        const auto leftEvent =
                            m_eventsById.constFind(leftId);
                        const auto rightEvent =
                            m_eventsById.constFind(rightId);
                        if (
                            leftEvent == m_eventsById.cend()
                            || rightEvent == m_eventsById.cend()
                            )
                        {
                            // The date index should only contain canonical
                            // IDs. Keep the comparator total if a stale
                            // membership is ever observed.
                            return leftEvent == m_eventsById.cend()
                                && rightEvent != m_eventsById.cend();
                        }

                        return summaryComesBeforeOnDate(
                            leftEvent.value(),
                            rightEvent.value()
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
        [this, startDate, endDate](const Request& request)
        {
            return request.kind == RequestKind::Range
                && request.generation == m_generation
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

    m_rangeFailures.erase(
        std::remove_if(
            m_rangeFailures.begin(),
            m_rangeFailures.end(),
            [this](const RangeFailure& failure)
            {
                return retainedRangesWithin(failure.range).isEmpty();
            }
            ),
        m_rangeFailures.end()
        );

    QList<DateRange> retainedLoadedRanges;
    for (const DateRange& loadedRange : m_loadedRanges)
    {
        retainedLoadedRanges.append(
            retainedRangesWithin(loadedRange)
            );
    }
    m_loadedRanges = normalizedRanges(retainedLoadedRanges);

    QSet<qint64> referencedEventIds;
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

        for (const qint64 eventId : iterator.value())
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
    qint64 eventId
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
