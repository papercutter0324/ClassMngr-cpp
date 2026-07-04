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
