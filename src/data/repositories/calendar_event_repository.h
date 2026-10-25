#pragma once

#include "data/repositories/repository_error.h"
#include "domain/models/calendar_event.h"

#include <functional>
#include <QDate>
#include <QList>
#include <QSqlDatabase>
#include <QString>

enum class CalendarEventRangeVisitDecision
{
    Continue,
    Stop
};

using CalendarEventRangeVisitor = std::function<
    CalendarEventRangeVisitDecision(const CalendarEvent&)
    >;

// Narrow source projection for import duplicate detection. Keep persistence
// data types here so repository callers do not depend on Application keys.
struct CalendarEventImportSignatureSourceRecord final
{
    QString title;
    QString eventType;
    QDate startDate;
    QDate endDate;
    bool allDay = false;
    QString timeStatus;
};

using CalendarEventImportSignatureSourceRangeVisitor = std::function<
    CalendarEventRangeVisitDecision(
        const CalendarEventImportSignatureSourceRecord&
        )
    >;

class CalendarEventRepository
{
public:
    explicit CalendarEventRepository(
        QSqlDatabase& database
        );

    [[nodiscard]] RepositoryResult<QList<CalendarEvent>> loadCalendarEventsForDate(
        const QDate& date
        );

    [[nodiscard]] RepositoryResult<QList<CalendarEvent>> loadCalendarEventsInRange(
        const QDate& startDate,
        const QDate& endDate
        );

    [[nodiscard]] RepositoryStatus visitCalendarEventsInRange(
        const QDate& startDate,
        const QDate& endDate,
        const CalendarEventRangeVisitor& visitor
        );

    [[nodiscard]] RepositoryStatus visitCalendarEventImportSignatureSourcesInRange(
        const QDate& startDate,
        const QDate& endDate,
        const CalendarEventImportSignatureSourceRangeVisitor& visitor
        );

    [[nodiscard]] RepositoryResult<QList<CalendarEventDateInterval>>
    loadCalendarEventDateIntervalsInRange(
        const QDate& startDate,
        const QDate& endDate
        );

    [[nodiscard]] RepositoryResult<QList<CalendarEvent>> loadUpcomingCalendarEvents(
        const QDate& fromDate,
        int limit
        );

    [[nodiscard]] RepositoryResult<QDate> findNextCalendarEventStartDate(
        const QDate& fromDate
        );

    [[nodiscard]] RepositoryResult<CalendarEvent> getCalendarEvent(
        qint64 eventId
        );
    [[nodiscard]] RepositoryResult<CalendarEvent> lookupCalendarEventById(
        qint64 eventId
        );

    [[nodiscard]] RepositoryResult<QList<CalendarEvent>> loadCalendarEventsForRepeatSeriesFromDate(
        const QString& repeatSeriesId,
        const QDate& startDate
        );

    [[nodiscard]] RepositoryResult<qint64> saveCalendarEvent(
        const CalendarEvent& event
        );
    [[nodiscard]] RepositoryResult<qint64> saveCalendarEventWithOutcome(
        const CalendarEvent& event
        );

    [[nodiscard]] RepositoryResult<QList<qint64>> saveCalendarEvents(
        const QList<CalendarEvent>& events
        );

    [[nodiscard]] RepositoryStatus deleteCalendarEvent(
        qint64 eventId
        );

    [[nodiscard]] RepositoryStatus deleteCalendarEventsForRepeatSeriesFromDate(
        const QString& repeatSeriesId,
        const QDate& startDate
        );

    [[nodiscard]] RepositoryStatus deleteAllCalendarEvents();

private:
    [[nodiscard]] RepositoryResult<qint64>
    saveCalendarEventWithoutTransaction(
        const CalendarEvent& event
        );

    QSqlDatabase& m_database;
};
