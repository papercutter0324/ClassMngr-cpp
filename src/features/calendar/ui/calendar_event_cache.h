#pragma once

#include "domain/models/calendar_event.h"
#include "next/application/calendar_event_query_port.h"
#include "next/application/calendar_event_projection.h"

#include <QDate>
#include <QFutureWatcher>
#include <QHash>
#include <QList>
#include <QObject>

#include <memory>
#include <optional>
#include <vector>

class CalendarEventCache : public QObject
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

    explicit CalendarEventCache(
        std::shared_ptr<
            const ClassMngr::Next::Application::CalendarEventQueryPortFactory
            > queryFactory,
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

    ClassMngr::Next::Application::CalendarEventProjection
    eventProjectionForDate(
        const QDate& date
        ) const;

    ClassMngr::Next::Application::CalendarEventProjection
    eventProjectionInRange(
        const QDate& startDate,
        const QDate& endDate
        ) const;

    // Compatibility reads retained for the Legacy Calendar cache consumers.
    // The Next page uses only the typed projection API above.
    [[nodiscard]] QList<CalendarEvent> eventsForDate(
        const QDate& date
        ) const;
    [[nodiscard]] QList<CalendarEvent> eventsInRange(
        const QDate& startDate,
        const QDate& endDate
        ) const;

    bool isRangeLoaded(
        const QDate& startDate,
        const QDate& endDate
        ) const;
    [[nodiscard]] QString rangeError(
        const QDate& startDate,
        const QDate& endDate
        ) const;
    [[nodiscard]] QString nextEventError(
        const QDate& afterDate
        ) const;
    bool isLoading() const;
    [[nodiscard]] int eventCount() const;
    [[nodiscard]] int dateBucketCount() const;
    [[nodiscard]] int loadedRangeCount() const;
    [[nodiscard]] int retainedRangeCount() const;

signals:
    void cacheChanged();
    void loadingChanged();
    void nextEventMonthFound(
        const QDate& firstEventDate
        );
    void nextEventMonthFailed(
        const QDate& afterDate
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

    struct RangeFailure
    {
        DateRange range;
        QString error;
    };

    struct NextEventFailure
    {
        QDate afterDate;
        QString error;
    };

    struct LoadResult
    {
        Request request;
        ClassMngr::Next::Application::CalendarEventProjection projection;
        QDate nextEventDate;
        QString error;
    };

    static LoadResult load(
        const std::shared_ptr<
            const ClassMngr::Next::Application::CalendarEventQueryPortFactory
            >& queryFactory,
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
        const std::vector<
            ClassMngr::Next::Application::CalendarEventSummary
            >& events,
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
        qint64 eventId
        );
    void emitLoadingChangedIfNeeded(
        bool previouslyLoading
        );

    QString m_databasePath;
    std::shared_ptr<
        const ClassMngr::Next::Application::CalendarEventQueryPortFactory
        > m_queryFactory;
    QHash<
        qint64,
        ClassMngr::Next::Application::CalendarEventSummary
        > m_eventsById;
    QHash<QDate, QList<qint64>> m_eventIdsByDate;
    quint64 m_dateIndexEntryCount = 0;
    QList<DateRange> m_loadedRanges;
    QList<RangeFailure> m_rangeFailures;
    std::optional<NextEventFailure> m_nextEventFailure;
    QList<DateRange> m_retainedRanges;
    bool m_retentionEnabled = false;
    QList<Request> m_pendingRequests;
    std::optional<Request> m_activeRequest;
    QFutureWatcher<LoadResult> m_watcher;
    quint64 m_generation = 0;
};
