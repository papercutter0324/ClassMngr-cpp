#include "next/application/sub_prep_navigation_schedule_order_policy.h"

#include <cstdlib>
#include <iostream>
#include <optional>
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

bool earliestDayWithoutValidTimeRemainsSelected()
{
    const std::vector<SubPrepNavigationScheduleOrderFact> facts{
        {2, 570},
        {0, std::nullopt}
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
        {5, 100},
        {2, 400},
        {0, 570},
        {0, 500},
        {1, 300}
    };

    return calculateSubPrepNavigationScheduleOrder(facts)
        == std::pair{std::int32_t{0}, std::int32_t{500}};
}

bool numericGuardsIgnoreInvalidWeekdaysAndMinutes()
{
    const std::vector<SubPrepNavigationScheduleOrderFact> facts{
        {-1, 120},
        {7, 130},
        {0, -1},
        {0, 1'440},
        {2, 240}
    };

    return calculateSubPrepNavigationScheduleOrder(facts)
               == std::pair{
                   std::int32_t{0},
                   kClassSummaryUnknownNavigationOrder
               }
        && calculateSubPrepNavigationScheduleOrder({{-1, 120}, {7, 130}})
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
    if (!numericGuardsIgnoreInvalidWeekdaysAndMinutes())
    {
        std::cerr << "Invalid weekdays and out-of-range minutes should not affect schedule order.\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
