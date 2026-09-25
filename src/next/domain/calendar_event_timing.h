#pragma once

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

namespace ClassMngr::Next::Domain
{

enum class CalendarEventTimeStatus
{
    Timed,
    Unknown,
    Unconfirmed
};

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
        if (value.size() != 10)
        {
            return false;
        }

        for (std::size_t index = 0; index < value.size(); ++index)
        {
            if (index == 4 || index == 7)
            {
                if (value.at(index) != '-')
                {
                    return false;
                }

                continue;
            }

            if (value.at(index) < '0' || value.at(index) > '9')
            {
                return false;
            }
        }

        const int year =
            (value.at(0) - '0') * 1000
            + (value.at(1) - '0') * 100
            + (value.at(2) - '0') * 10
            + (value.at(3) - '0');
        const int month =
            (value.at(5) - '0') * 10
            + (value.at(6) - '0');
        const int day =
            (value.at(8) - '0') * 10
            + (value.at(9) - '0');
        if (year <= 0 || month < 1 || month > 12 || day < 1)
        {
            return false;
        }

        constexpr int daysInMonth[] = {
            31, 28, 31, 30, 31, 30,
            31, 31, 30, 31, 30, 31
        };
        int maximumDay = daysInMonth[month - 1];
        if (month == 2
            && (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)))
        {
            maximumDay = 29;
        }

        return day <= maximumDay;
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
