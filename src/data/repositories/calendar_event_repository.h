#pragma once

#include "core/result.h"
#include "domain/models/calendar_event.h"

#include <functional>
#include <QList>
#include <QSqlDatabase>
#include <QString>

enum class CalendarEventByIdLookupCode
{
    Found,
    Missing,
    Failure
};

struct CalendarEventByIdLookupResult final
{
    CalendarEventByIdLookupCode code = CalendarEventByIdLookupCode::Failure;
    CalendarEvent event;
    QString diagnostic;
};

enum class CalendarEventSaveOutcomeCode
{
    Saved,
    Missing,
    Failure
};

struct CalendarEventSaveOutcome final
{
    CalendarEventSaveOutcomeCode code = CalendarEventSaveOutcomeCode::Failure;
    qint64 eventId = 0;
    QString diagnostic;
};

enum class CalendarEventRangeVisitDecision
{
    Continue,
    Stop
};

using CalendarEventRangeVisitor = std::function<
    CalendarEventRangeVisitDecision(const CalendarEvent&)
    >;

class CalendarEventRepository
{
public:
    explicit CalendarEventRepository(
        QSqlDatabase& database
        );

    [[nodiscard]] Result<QList<CalendarEvent>> loadCalendarEventsForDate(
        const QDate& date
        );

    [[nodiscard]] Result<QList<CalendarEvent>> loadCalendarEventsInRange(
        const QDate& startDate,
        const QDate& endDate
        );

    [[nodiscard]] Status visitCalendarEventsInRange(
        const QDate& startDate,
        const QDate& endDate,
        const CalendarEventRangeVisitor& visitor
        );

    [[nodiscard]] Result<QList<CalendarEventDateInterval>>
    loadCalendarEventDateIntervalsInRange(
        const QDate& startDate,
        const QDate& endDate
        );

    [[nodiscard]] Result<QList<CalendarEvent>> loadUpcomingCalendarEvents(
        const QDate& fromDate,
        int limit
        );

    [[nodiscard]] Result<QDate> findNextCalendarEventStartDate(
        const QDate& fromDate
        );

    [[nodiscard]] Result<CalendarEvent> getCalendarEvent(
        qint64 eventId
        );
    [[nodiscard]] CalendarEventByIdLookupResult lookupCalendarEventById(
        qint64 eventId
        );

    [[nodiscard]] Result<QList<CalendarEvent>> loadCalendarEventsForRepeatSeriesFromDate(
        const QString& repeatSeriesId,
        const QDate& startDate
        );

    [[nodiscard]] Result<qint64> saveCalendarEvent(
        const CalendarEvent& event
        );
    [[nodiscard]] CalendarEventSaveOutcome saveCalendarEventWithOutcome(
        const CalendarEvent& event
        );

    [[nodiscard]] Result<QList<qint64>> saveCalendarEvents(
        const QList<CalendarEvent>& events
        );

    [[nodiscard]] Status deleteCalendarEvent(
        qint64 eventId
        );

    [[nodiscard]] Status deleteCalendarEventsForRepeatSeriesFromDate(
        const QString& repeatSeriesId,
        const QDate& startDate
        );

    [[nodiscard]] Status deleteAllCalendarEvents();

private:
    [[nodiscard]] CalendarEventSaveOutcome
    saveCalendarEventOutcomeWithoutTransaction(
        const CalendarEvent& event
        );

    QSqlDatabase& m_database;
};
