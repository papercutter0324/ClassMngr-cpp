#pragma once

#include "next/application/sub_prep_weekday.h"

#include <algorithm>
#include <cstdint>
#include <set>
#include <vector>

namespace ClassMngr::Next::Application
{

class SubPrepMeetingSecondsOfDay final
{
public:
    SubPrepMeetingSecondsOfDay() = default;

    explicit constexpr SubPrepMeetingSecondsOfDay(
        const std::int32_t value
        ) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr std::int32_t value() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const SubPrepMeetingSecondsOfDay&,
        const SubPrepMeetingSecondsOfDay&
        ) = default;

private:
    std::int32_t m_value = 0;
};

// A meeting time fact has already passed platform parsing and canonical
// weekday checks. Seconds preserve distinctions in parsed time values that
// share the same hour and minute.
struct SubPrepMeetingTimeFact final
{
    SubPrepWeekday weekday = SubPrepWeekday::Monday;
    SubPrepMeetingSecondsOfDay secondsOfDay;

    friend bool operator==(
        const SubPrepMeetingTimeFact&,
        const SubPrepMeetingTimeFact&
        ) = default;
};

struct SubPrepMeetingTimeGroup final
{
    SubPrepMeetingSecondsOfDay secondsOfDay;
    std::vector<SubPrepWeekday> weekdays;

    friend bool operator==(
        const SubPrepMeetingTimeGroup&,
        const SubPrepMeetingTimeGroup&
        ) = default;
};

// Filters resolved meeting facts to the selected weekdays, groups equal
// exact times across weekdays, and returns canonical weekday and group order.
[[nodiscard]] inline std::vector<SubPrepMeetingTimeGroup>
groupSelectedSubPrepMeetingTimes(
    const std::vector<SubPrepMeetingTimeFact>& meetings,
    const std::vector<SubPrepWeekday>& selectedWeekdays
    )
{
    std::set<SubPrepWeekday> selected;
    for (const SubPrepWeekday weekday : selectedWeekdays)
    {
        if (isValidSubPrepWeekday(weekday))
        {
            selected.insert(weekday);
        }
    }

    std::vector<SubPrepMeetingTimeGroup> groups;
    for (const SubPrepMeetingTimeFact& meeting : meetings)
    {
        if (!isValidSubPrepWeekday(meeting.weekday)
            || !selected.contains(meeting.weekday)
            || meeting.secondsOfDay.value() < 0
            || meeting.secondsOfDay.value() >= 24 * 60 * 60)
        {
            continue;
        }

        const auto group = std::find_if(
            groups.begin(),
            groups.end(),
            [&meeting](const SubPrepMeetingTimeGroup& candidate)
            {
                return candidate.secondsOfDay == meeting.secondsOfDay;
            }
            );
        if (group == groups.end())
        {
            groups.push_back({meeting.secondsOfDay, {meeting.weekday}});
        }
        else if (std::find(
                     group->weekdays.begin(),
                     group->weekdays.end(),
                     meeting.weekday
                     ) == group->weekdays.end())
        {
            group->weekdays.push_back(meeting.weekday);
        }
    }

    for (SubPrepMeetingTimeGroup& group : groups)
    {
        std::sort(group.weekdays.begin(), group.weekdays.end());
    }
    std::sort(
        groups.begin(),
        groups.end(),
        [](const SubPrepMeetingTimeGroup& left,
           const SubPrepMeetingTimeGroup& right)
        {
            if (left.weekdays.front() != right.weekdays.front())
            {
                return left.weekdays.front() < right.weekdays.front();
            }
            return left.secondsOfDay.value() < right.secondsOfDay.value();
        }
        );
    return groups;
}

} // namespace ClassMngr::Next::Application
