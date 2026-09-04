#pragma once

#include "classmngr/engine/calendar_event.h"
#include "classmngr/engine/calendar_event_rules.h"

#include <QByteArray>
#include <QDate>
#include <QString>
#include <QStringList>
#include <QTime>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

using CalendarEventRepeatFrequency =
    classmngr::engine::CalendarEventRepeatFrequency;

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

struct CalendarEventImportSummary
{
    int importedCount = 0;
    int skippedCount = 0;
};

namespace calendar_event_detail
{
inline std::string toUtf8(const QString& value)
{
    const QByteArray encoded = value.toUtf8();
    return {
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
    };
}

inline QString fromUtf8(std::string_view value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

inline classmngr::engine::CalendarDate toEngineDate(const QDate& date)
{
    if (!date.isValid())
    {
        return {};
    }

    return {
        std::chrono::year(date.year()),
        std::chrono::month(static_cast<unsigned>(date.month())),
        std::chrono::day(static_cast<unsigned>(date.day()))
    };
}

inline QDate fromEngineDate(
    const classmngr::engine::CalendarDate& date
    )
{
    if (!date.ok())
    {
        return {};
    }

    return QDate(
        static_cast<int>(date.year()),
        static_cast<int>(static_cast<unsigned>(date.month())),
        static_cast<int>(static_cast<unsigned>(date.day()))
        );
}

inline std::optional<std::chrono::minutes> toEngineTime(const QTime& time)
{
    if (!time.isValid())
    {
        return std::nullopt;
    }

    return std::chrono::minutes{time.hour() * 60 + time.minute()};
}

inline QTime fromEngineTime(
    const std::optional<std::chrono::minutes>& time
    )
{
    if (!time)
    {
        return {};
    }

    const auto count = time->count();
    if (count < 0 || count >= 24 * 60)
    {
        return {};
    }

    return QTime(
        static_cast<int>(count / 60),
        static_cast<int>(count % 60)
        );
}
} // namespace calendar_event_detail

inline classmngr::engine::CalendarEvent calendarEventToEngine(
    const CalendarEvent& event
    )
{
    classmngr::engine::CalendarEvent result;
    result.id = event.id;
    result.title = calendar_event_detail::toUtf8(event.title);
    result.eventType = calendar_event_detail::toUtf8(event.eventType);
    result.timeStatus = calendar_event_detail::toUtf8(event.timeStatus);
    result.repeatSeriesId = calendar_event_detail::toUtf8(event.repeatSeriesId);
    result.allDay = event.allDay;
    result.startDate = calendar_event_detail::toEngineDate(event.startDate);
    result.startTime = calendar_event_detail::toEngineTime(event.startTime);
    result.endDate = calendar_event_detail::toEngineDate(event.endDate);
    result.endTime = calendar_event_detail::toEngineTime(event.endTime);
    return result;
}

inline CalendarEvent calendarEventFromEngine(
    const classmngr::engine::CalendarEvent& event
    )
{
    CalendarEvent result;
    result.id = event.id;
    result.title = calendar_event_detail::fromUtf8(event.title);
    result.eventType = calendar_event_detail::fromUtf8(event.eventType);
    result.timeStatus = calendar_event_detail::fromUtf8(event.timeStatus);
    result.repeatSeriesId = calendar_event_detail::fromUtf8(event.repeatSeriesId);
    result.allDay = event.allDay;
    result.startDate = calendar_event_detail::fromEngineDate(event.startDate);
    result.startTime = calendar_event_detail::fromEngineTime(event.startTime);
    result.endDate = calendar_event_detail::fromEngineDate(event.endDate);
    result.endTime = calendar_event_detail::fromEngineTime(event.endTime);
    return result;
}

inline QStringList calendarEventTypes()
{
    QStringList result;
    for (const std::string_view eventType
         : classmngr::engine::CalendarEventRules::eventTypes())
    {
        result.append(calendar_event_detail::fromUtf8(eventType));
    }
    return result;
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
    QStringList result;
    for (const std::string_view timeStatus
         : classmngr::engine::CalendarEventRules::timeStatuses())
    {
        result.append(calendar_event_detail::fromUtf8(timeStatus));
    }
    return result;
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
