#pragma once

#include "classmngr/engine/calendar_event_rules.h"

#include <QDate>
#include <QString>
#include <QStringList>
#include <QTime>

enum class CalendarEventRepeatFrequency
{
    Daily,
    Weekly,
    Monthly
};

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
    const std::string normalized =
        classmngr::engine::CalendarEventRules::normalizedEventType(
            eventType.toUtf8().toStdString()
            );
    return QString::fromUtf8(normalized.c_str());
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
    const std::string normalized =
        classmngr::engine::CalendarEventRules::normalizedTimeStatus(
            timeStatus.toUtf8().toStdString()
            );
    return QString::fromUtf8(normalized.c_str());
}

inline bool isStartOfTermCalendarEvent(
    const CalendarEvent& event
    )
{
    return classmngr::engine::CalendarEventRules::isStartOfTerm(
        event.title.toUtf8().toStdString(),
        event.eventType.toUtf8().toStdString()
        );
}
