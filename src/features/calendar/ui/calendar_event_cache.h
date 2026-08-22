#pragma once

#include "core/memory_usage_diagnostics.h"
#include "domain/models/calendar_event.h"

#include <QDate>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QHash>
#include <QList>
#include <QObject>

#include <optional>

class CalendarEventCache : public QObject, public MemoryBreakdownProvider
{
    Q_OBJECT

public:
    struct DateRange
    {
        QDate startDate;
        QDate endDate;

        [[nodiscard]] bool operator==(
            const DateRange& other
            ) const = default;
    };

    enum class Priority
    {
        Foreground,
        Background
    };

    explicit CalendarEventCache(
        QObject* parent = nullptr
        );

    void setDatabasePath(
        const QString& databasePath
        );
    QString databasePath() const;

    void invalidate();

    void setRetainedRanges(
        const QList<DateRange>& ranges
        );

    void requestRange(
        const QDate& startDate,
        const QDate& endDate,
        Priority priority = Priority::Foreground
        );
    void requestNextEventMonth(
        const QDate& afterDate,
        Priority priority = Priority::Foreground
        );

    QList<CalendarEvent> eventsForDate(
        const QDate& date
        ) const;
    QList<CalendarEvent> eventsInRange(
        const QDate& startDate,
        const QDate& endDate
        ) const;

    bool isRangeLoaded(
        const QDate& startDate,
        const QDate& endDate
        ) const;
    bool isLoading() const;
    [[nodiscard]] int eventCount() const;
    [[nodiscard]] int dateBucketCount() const;
    [[nodiscard]] QList<MemoryBreakdownEntry>
        memoryBreakdown() const override;

signals:
    void cacheChanged();
    void loadingChanged();
    void nextEventMonthFound(
        const QDate& firstEventDate
        );

private:
    enum class RequestKind
    {
        Range,
        NextEventMonth
    };

    struct Request
    {
        RequestKind kind = RequestKind::Range;
        QDate startDate;
        QDate endDate;
        quint64 generation = 0;
        Priority priority = Priority::Foreground;
    };

    struct LoadResult
    {
        Request request;
        QList<CalendarEvent> events;
        QDate nextEventDate;
        QString error;
    };

    static LoadResult load(
        const QString& databasePath,
        const Request& request
        );

    void enqueue(
        const Request& request,
        Priority priority
        );
    void startNextRequest();
    void finishActiveRequest();
    void insertEvents(
        const QList<CalendarEvent>& events,
        const QList<DateRange>& loadedRanges
        );
    void markRangeLoaded(
        const QDate& startDate,
        const QDate& endDate
        );
    bool hasPendingRange(
        const QDate& startDate,
        const QDate& endDate
        ) const;
    QList<DateRange> retainedRangesWithin(
        const DateRange& range
        ) const;
    bool isDateRetained(
        const QDate& date
        ) const;
    void pruneToRetainedRanges();
    void removeEventMemberships(
        int eventId
        );
    void emitLoadingChangedIfNeeded(
        bool previouslyLoading
        );

    QString m_databasePath;
    QHash<int, CalendarEvent> m_eventsById;
    QHash<QDate, QList<int>> m_eventIdsByDate;
    quint64 m_dateIndexEntryCount = 0;
    QList<DateRange> m_loadedRanges;
    QList<DateRange> m_retainedRanges;
    bool m_retentionEnabled = false;
    QList<Request> m_pendingRequests;
    std::optional<Request> m_activeRequest;
    QFutureWatcher<LoadResult> m_watcher;
    QElapsedTimer m_activeRequestTimer;
    bool m_activeRequestTiming = false;
    quint64 m_activeDiagnosticTaskId = 0;
    quint64 m_generation = 0;
};
