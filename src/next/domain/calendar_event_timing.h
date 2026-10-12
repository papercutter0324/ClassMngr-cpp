#pragma once

#include "next/domain/gregorian_date.h"

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

namespace ClassMngr::Next::Domain
{

enum class CalendarEventType
{
    Vacation,
    Holiday,
    Workshop,
    CM,
    Meeting,
    Other
};

enum class CalendarEventTimeStatus
{
    Timed,
    Unknown,
    Unconfirmed
};

// These classifiers recognize canonical persisted names. Callers that accept
// surrounding whitespace should trim at their own boundary before parsing.
[[nodiscard]] inline std::optional<CalendarEventType>
calendarEventTypeFromName(
    const std::string_view name
    ) noexcept
{
    if (name == "Vacation")
    {
        return CalendarEventType::Vacation;
    }
    if (name == "Holiday")
    {
        return CalendarEventType::Holiday;
    }
    if (name == "Workshop")
    {
        return CalendarEventType::Workshop;
    }
    if (name == "CM")
    {
        return CalendarEventType::CM;
    }
    if (name == "Meeting")
    {
        return CalendarEventType::Meeting;
    }
    if (name == "Other")
    {
        return CalendarEventType::Other;
    }

    return std::nullopt;
}

[[nodiscard]] inline std::optional<CalendarEventTimeStatus>
calendarEventTimeStatusFromName(
    const std::string_view name
    ) noexcept
{
    if (name == "Timed")
    {
        return CalendarEventTimeStatus::Timed;
    }
    if (name == "Unknown")
    {
        return CalendarEventTimeStatus::Unknown;
    }
    if (name == "Unconfirmed")
    {
        return CalendarEventTimeStatus::Unconfirmed;
    }

    return std::nullopt;
}

enum class CalendarEventTimingIssue
{
    InvalidDate,
    EndDateBeforeStartDate,
    TimesMustBePaired,
    InvalidTime,
    AllDayRequiresTimedStatusAndNoTimes,
    TimedRequiresBothTimes,
    EndTimeMustFollowStartTime,
    NonTimedStatusRequiresNoTimes
};

// A value describing one calendar event's date and time range. Date and time
// text stays in canonical ISO form so platform-specific conversion remains
// outside the Domain layer.
class CalendarEventTiming final
{
public:
    CalendarEventTiming(
        const std::string_view startDate,
        const std::string_view endDate,
        const std::optional<std::string>& startTime,
        const std::optional<std::string>& endTime,
        const bool allDay,
        const CalendarEventTimeStatus timeStatus
        )
        : m_startDate(startDate)
        , m_endDate(endDate)
        , m_startTime(startTime)
        , m_endTime(endTime)
        , m_allDay(allDay)
        , m_timeStatus(timeStatus)
    {
    }

    [[nodiscard]] static bool isCanonicalDate(
        const std::string_view value
        ) noexcept
    {
        return GregorianDate::fromIsoString(value).has_value();
    }

    [[nodiscard]] std::optional<CalendarEventTimingIssue> validate() const
    {
        if (!isCanonicalDate(m_startDate) || !isCanonicalDate(m_endDate))
        {
            return CalendarEventTimingIssue::InvalidDate;
        }

        if (m_endDate < m_startDate)
        {
            return CalendarEventTimingIssue::EndDateBeforeStartDate;
        }

        const bool hasStartTime = m_startTime.has_value();
        const bool hasEndTime = m_endTime.has_value();
        if (hasStartTime != hasEndTime)
        {
            return CalendarEventTimingIssue::TimesMustBePaired;
        }

        if ((hasStartTime && !isCanonicalTime(*m_startTime))
            || (hasEndTime && !isCanonicalTime(*m_endTime)))
        {
            return CalendarEventTimingIssue::InvalidTime;
        }

        if (m_allDay)
        {
            if (m_timeStatus != CalendarEventTimeStatus::Timed
                || hasStartTime
                || hasEndTime)
            {
                return CalendarEventTimingIssue::
                    AllDayRequiresTimedStatusAndNoTimes;
            }

            return std::nullopt;
        }

        if (m_timeStatus == CalendarEventTimeStatus::Timed)
        {
            if (!hasStartTime || !hasEndTime)
            {
                return CalendarEventTimingIssue::TimedRequiresBothTimes;
            }

            if (m_startDate == m_endDate
                && *m_endTime <= *m_startTime)
            {
                return CalendarEventTimingIssue::EndTimeMustFollowStartTime;
            }
        }
        else if (hasStartTime || hasEndTime)
        {
            return CalendarEventTimingIssue::NonTimedStatusRequiresNoTimes;
        }

        return std::nullopt;
    }

    [[nodiscard]] const std::string& startDate() const noexcept
    {
        return m_startDate;
    }

    [[nodiscard]] const std::string& endDate() const noexcept
    {
        return m_endDate;
    }

    [[nodiscard]] const std::optional<std::string>& startTime() const noexcept
    {
        return m_startTime;
    }

    [[nodiscard]] const std::optional<std::string>& endTime() const noexcept
    {
        return m_endTime;
    }

    [[nodiscard]] bool isAllDay() const noexcept
    {
        return m_allDay;
    }

    [[nodiscard]] CalendarEventTimeStatus timeStatus() const noexcept
    {
        return m_timeStatus;
    }

    friend bool operator==(
        const CalendarEventTiming&,
        const CalendarEventTiming&
        ) = default;

private:
    [[nodiscard]] static bool isCanonicalTime(
        const std::string_view value
        ) noexcept
    {
        if (value.size() != 5 || value.at(2) != ':')
        {
            return false;
        }

        for (const std::size_t index : {0U, 1U, 3U, 4U})
        {
            if (value.at(index) < '0' || value.at(index) > '9')
            {
                return false;
            }
        }

        const int hour =
            (value.at(0) - '0') * 10 + value.at(1) - '0';
        const int minute =
            (value.at(3) - '0') * 10 + value.at(4) - '0';
        return hour <= 23 && minute <= 59;
    }

    std::string m_startDate;
    std::string m_endDate;
    std::optional<std::string> m_startTime;
    std::optional<std::string> m_endTime;
    bool m_allDay;
    CalendarEventTimeStatus m_timeStatus;
};

} // namespace ClassMngr::Next::Domain
