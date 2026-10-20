#include "next/application/sub_prep_navigation_schedule_order_policy.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next::Application;

namespace
{

bool emptyFactsReturnUnknownDayAndTime()
{
    return calculateSubPrepNavigationScheduleOrder({})
        == std::pair{
            kClassSummaryUnknownNavigationOrder,
            kClassSummaryUnknownNavigationOrder
        };
}

bool typedWeekdayAndMinuteDefaultsAndEqualityAreExplicit()
{
    using Minute = SubPrepNavigationStartMinute;
    using Fact = SubPrepNavigationScheduleOrderFact;
    static_assert(!std::is_convertible_v<std::int32_t, Minute>);
    static_assert(!std::is_convertible_v<Minute, std::int32_t>);
    static_assert(std::is_constructible_v<Minute, std::int32_t>);

    const Minute defaultMinute;
    const Fact defaultFact;
    return defaultMinute.value() == 0
        && Minute{0} == defaultMinute
        && Minute{1} == Minute{1}
        && Minute{1} != Minute{2}
        && defaultFact.weekdayOrder == ClassMngr::Next::Domain::Weekday::Monday
        && !defaultFact.startMinute;
}

bool earliestDayWithoutValidTimeRemainsSelected()
{
    const std::vector<SubPrepNavigationScheduleOrderFact> facts{
        {ClassMngr::Next::Domain::Weekday::Wednesday,
         SubPrepNavigationStartMinute{570}},
        {ClassMngr::Next::Domain::Weekday::Monday, std::nullopt}
    };

    return calculateSubPrepNavigationScheduleOrder(facts)
        == std::pair{
            std::int32_t{0},
            kClassSummaryUnknownNavigationOrder
        };
}

bool earliestMinuteOnEarliestDayIgnoresInputOrder()
{
    const std::vector<SubPrepNavigationScheduleOrderFact> facts{
        {ClassMngr::Next::Domain::Weekday::Saturday,
         SubPrepNavigationStartMinute{100}},
        {ClassMngr::Next::Domain::Weekday::Wednesday,
         SubPrepNavigationStartMinute{400}},
        {ClassMngr::Next::Domain::Weekday::Monday,
         SubPrepNavigationStartMinute{570}},
        {ClassMngr::Next::Domain::Weekday::Monday,
         SubPrepNavigationStartMinute{500}},
        {ClassMngr::Next::Domain::Weekday::Tuesday,
         SubPrepNavigationStartMinute{300}}
    };

    return calculateSubPrepNavigationScheduleOrder(facts)
        == std::pair{std::int32_t{0}, std::int32_t{500}};
}

bool weekdayAndMinuteBoundariesAreAccepted()
{
    using ClassMngr::Next::Domain::Weekday;
    const std::vector<SubPrepNavigationScheduleOrderFact> mondayFacts{
        {Weekday::Monday, SubPrepNavigationStartMinute{0}},
        {Weekday::Sunday, SubPrepNavigationStartMinute{1'439}}
    };
    const std::vector<SubPrepNavigationScheduleOrderFact> sundayFacts{
        {Weekday::Sunday, SubPrepNavigationStartMinute{1'439}}
    };

    const auto mondayOrder =
        calculateSubPrepNavigationScheduleOrder(mondayFacts);
    const auto sundayOrder =
        calculateSubPrepNavigationScheduleOrder(sundayFacts);
    return mondayOrder == std::pair{std::int32_t{0}, std::int32_t{0}}
        && sundayOrder == std::pair{std::int32_t{6}, std::int32_t{1'439}};
}

bool numericGuardsIgnoreInvalidWeekdaysAndMinutes()
{
    using ClassMngr::Next::Domain::Weekday;
    const std::vector<SubPrepNavigationScheduleOrderFact> invalidWeekdays{
        {static_cast<Weekday>(-1), SubPrepNavigationStartMinute{120}},
        {static_cast<Weekday>(7), SubPrepNavigationStartMinute{130}},
        {Weekday::Wednesday, SubPrepNavigationStartMinute{240}}
    };
    const std::vector<SubPrepNavigationScheduleOrderFact> invalidMinutes{
        {Weekday::Monday, SubPrepNavigationStartMinute{-1}},
        {Weekday::Monday, SubPrepNavigationStartMinute{1'440}},
        {Weekday::Tuesday, SubPrepNavigationStartMinute{240}}
    };

    return calculateSubPrepNavigationScheduleOrder(invalidWeekdays)
               == std::pair{
                   std::int32_t{2},
                   std::int32_t{240}
               }
        && calculateSubPrepNavigationScheduleOrder(invalidMinutes)
               == std::pair{
                   std::int32_t{0},
                   kClassSummaryUnknownNavigationOrder
               }
        && calculateSubPrepNavigationScheduleOrder({
               {static_cast<Weekday>(-1),
                SubPrepNavigationStartMinute{120}},
               {static_cast<Weekday>(7),
                SubPrepNavigationStartMinute{130}}
           })
               == std::pair{
                   kClassSummaryUnknownNavigationOrder,
                   kClassSummaryUnknownNavigationOrder
               };
}

} // namespace

int main()
{
    if (!emptyFactsReturnUnknownDayAndTime())
    {
        std::cerr << "Empty schedule-order facts should return unknown day and time.\n";
        return EXIT_FAILURE;
    }
    if (!typedWeekdayAndMinuteDefaultsAndEqualityAreExplicit())
    {
        std::cerr << "Navigation fact weekdays and minutes should be strongly typed.\n";
        return EXIT_FAILURE;
    }
    if (!earliestDayWithoutValidTimeRemainsSelected())
    {
        std::cerr << "An earliest day without a valid time should retain unknown time order.\n";
        return EXIT_FAILURE;
    }
    if (!earliestMinuteOnEarliestDayIgnoresInputOrder())
    {
        std::cerr << "Schedule order should select the earliest minute on the earliest day.\n";
        return EXIT_FAILURE;
    }
    if (!weekdayAndMinuteBoundariesAreAccepted())
    {
        std::cerr << "Monday/Sunday and minute 0/1439 should remain valid boundaries.\n";
        return EXIT_FAILURE;
    }
    if (!numericGuardsIgnoreInvalidWeekdaysAndMinutes())
    {
        std::cerr << "Invalid weekdays and out-of-range minutes should not affect schedule order.\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
