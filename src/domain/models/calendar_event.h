#pragma once

#include <QDate>
#include <QString>
#include <QStringList>
#include <QTime>

struct CalendarEvent
{
    int id = -1;
    QString title;
    QString eventType = QStringLiteral("Other");
    QString timeStatus = QStringLiteral("Timed");
    QString repeatSeriesId;
    bool allDay = false;
    QDate startDate;
    QTime startTime;
    QDate endDate;
    QTime endTime;
};

inline QStringList calendarEventTypes()
{
    return {
        QStringLiteral("Vacation"),
        QStringLiteral("Holiday"),
        QStringLiteral("Workshop"),
        QStringLiteral("CM"),
        QStringLiteral("Meeting"),
        QStringLiteral("Other")
    };
}

inline QString normalizedCalendarEventType(
    const QString& eventType
    )
{
    const QString trimmed =
        eventType.trimmed();

    return calendarEventTypes().contains(trimmed)
        ? trimmed
        : QStringLiteral("Other");
}

inline QStringList calendarEventTimeStatuses()
{
    return {
        QStringLiteral("Timed"),
        QStringLiteral("Unknown"),
        QStringLiteral("Unconfirmed")
    };
}

inline QString normalizedCalendarEventTimeStatus(
    const QString& timeStatus
    )
{
    const QString trimmed =
        timeStatus.trimmed();

    return calendarEventTimeStatuses().contains(trimmed)
        ? trimmed
        : QStringLiteral("Timed");
}
