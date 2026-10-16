#include "next/application/sub_prep_meeting_time_grouping_policy.h"

#include <cstdlib>
#include <iostream>
#include <vector>

using namespace ClassMngr::Next::Application;

namespace
{

constexpr std::int32_t seconds(
    const int hour,
    const int minute,
    const int second = 0
    ) noexcept
{
    return ((hour * 60) + minute) * 60 + second;
}

bool selectedDayFilteringKeepsOnlyTypedSelectedWeekdays()
{
    const auto groups = groupSelectedSubPrepMeetingTimes(
        {
            {SubPrepWeekday::Monday, seconds(9, 0)},
            {SubPrepWeekday::Friday, seconds(10, 0)},
            {SubPrepWeekday::Tuesday, seconds(11, 0)}
        },
        {SubPrepWeekday::Tuesday, SubPrepWeekday::Friday}
        );

    return groups == std::vector<SubPrepMeetingTimeGroup>{
        {seconds(11, 0), {SubPrepWeekday::Tuesday}},
        {seconds(10, 0), {SubPrepWeekday::Friday}}
    };
}

bool reverseInputProducesCanonicalWeekdayAndTimeOrder()
{
    const auto groups = groupSelectedSubPrepMeetingTimes(
        {
            {SubPrepWeekday::Sunday, seconds(12, 0)},
            {SubPrepWeekday::Friday, seconds(9, 30)},
            {SubPrepWeekday::Tuesday, seconds(8, 0)},
            {SubPrepWeekday::Monday, seconds(10, 0)},
            {SubPrepWeekday::Monday, seconds(9, 0)}
        },
        {
            SubPrepWeekday::Sunday,
            SubPrepWeekday::Friday,
            SubPrepWeekday::Tuesday,
            SubPrepWeekday::Monday
        }
        );

    return groups == std::vector<SubPrepMeetingTimeGroup>{
        {seconds(9, 0), {SubPrepWeekday::Monday}},
        {seconds(10, 0), {SubPrepWeekday::Monday}},
        {seconds(8, 0), {SubPrepWeekday::Tuesday}},
        {seconds(9, 30), {SubPrepWeekday::Friday}},
        {seconds(12, 0), {SubPrepWeekday::Sunday}}
    };
}

bool equalTimesGroupAcrossDaysInCanonicalDayOrder()
{
    const auto groups = groupSelectedSubPrepMeetingTimes(
        {
            {SubPrepWeekday::Thursday, seconds(14, 30)},
            {SubPrepWeekday::Monday, seconds(14, 30)},
            {SubPrepWeekday::Tuesday, seconds(13, 0)}
        },
        {
            SubPrepWeekday::Thursday,
            SubPrepWeekday::Tuesday,
            SubPrepWeekday::Monday
        }
        );

    return groups == std::vector<SubPrepMeetingTimeGroup>{
        {
            seconds(14, 30),
            {SubPrepWeekday::Monday, SubPrepWeekday::Thursday}
        },
        {seconds(13, 0), {SubPrepWeekday::Tuesday}}
    };
}

bool repeatedDayInOneTimeGroupIsDeduplicated()
{
    const auto groups = groupSelectedSubPrepMeetingTimes(
        {
            {SubPrepWeekday::Wednesday, seconds(15, 0)},
            {SubPrepWeekday::Wednesday, seconds(15, 0)},
            {SubPrepWeekday::Wednesday, seconds(15, 0)}
        },
        {SubPrepWeekday::Wednesday}
        );

    return groups == std::vector<SubPrepMeetingTimeGroup>{
        {seconds(15, 0), {SubPrepWeekday::Wednesday}}
    };
}

bool emptyInputsOrNoSelectedMeetingsProduceNoGroups()
{
    return groupSelectedSubPrepMeetingTimes({}, {SubPrepWeekday::Monday})
            .empty()
        && groupSelectedSubPrepMeetingTimes(
               {{SubPrepWeekday::Monday, seconds(9, 0)}},
               {}
               ).empty()
        && groupSelectedSubPrepMeetingTimes(
               {{SubPrepWeekday::Monday, seconds(9, 0)}},
               {SubPrepWeekday::Tuesday}
               ).empty();
}

bool timesThatDifferOnlyBySecondsRemainSeparateAndOrdered()
{
    const auto groups = groupSelectedSubPrepMeetingTimes(
        {
            {SubPrepWeekday::Monday, seconds(9, 0, 2)},
            {SubPrepWeekday::Monday, seconds(9, 0, 1)}
        },
        {SubPrepWeekday::Monday}
        );

    return groups == std::vector<SubPrepMeetingTimeGroup>{
        {seconds(9, 0, 1), {SubPrepWeekday::Monday}},
        {seconds(9, 0, 2), {SubPrepWeekday::Monday}}
    };
}

} // namespace

int main()
{
    if (!selectedDayFilteringKeepsOnlyTypedSelectedWeekdays())
    {
        std::cerr << "Sub Prep meeting grouping should filter selected days.\n";
        return EXIT_FAILURE;
    }
    if (!reverseInputProducesCanonicalWeekdayAndTimeOrder())
    {
        std::cerr << "Sub Prep meeting groups should use canonical weekday and time order.\n";
        return EXIT_FAILURE;
    }
    if (!equalTimesGroupAcrossDaysInCanonicalDayOrder())
    {
        std::cerr << "Equal Sub Prep meeting times should group across canonical weekdays.\n";
        return EXIT_FAILURE;
    }
    if (!repeatedDayInOneTimeGroupIsDeduplicated())
    {
        std::cerr << "Sub Prep weekdays should be unique within a time group.\n";
        return EXIT_FAILURE;
    }
    if (!emptyInputsOrNoSelectedMeetingsProduceNoGroups())
    {
        std::cerr << "Empty Sub Prep meeting results should contain no groups.\n";
        return EXIT_FAILURE;
    }
    if (!timesThatDifferOnlyBySecondsRemainSeparateAndOrdered())
    {
        std::cerr << "Sub Prep meeting grouping should preserve exact seconds.\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
