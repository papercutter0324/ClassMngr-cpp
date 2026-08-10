#pragma once

#include "domain/models/calendar_event.h"

#include <QDate>
#include <QFutureWatcher>
#include <QHash>
#include <QList>
#include <QObject>

#include <optional>

class CalendarEventCache : public QObject
{
    Q_OBJECT

public:
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

signals:
    void cacheChanged();
    void loadingChanged();
    void nextEventMonthFound(
        const QDate& firstEventDate
        );

private:
    struct DateRange
    {
        QDate startDate;
        QDate endDate;
    };

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
        const QDate& loadedStartDate,
        const QDate& loadedEndDate
        );
    void markRangeLoaded(
        const QDate& startDate,
        const QDate& endDate
        );
    bool hasPendingRange(
        const QDate& startDate,
        const QDate& endDate
        ) const;
    void emitLoadingChangedIfNeeded(
        bool previouslyLoading
        );

    QString m_databasePath;
    QHash<int, CalendarEvent> m_eventsById;
    QHash<QDate, QList<CalendarEvent>> m_eventsByDate;
    QList<DateRange> m_loadedRanges;
    QList<Request> m_pendingRequests;
    std::optional<Request> m_activeRequest;
    QFutureWatcher<LoadResult> m_watcher;
    quint64 m_generation = 0;
};
