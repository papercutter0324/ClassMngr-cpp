#pragma once

#include "next/application/class_summary_projection.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

// A schedule-order fact has already passed Platform's exact weekday and raw
// time parsing. Weekday order is Monday=0 through Sunday=6; an absent minute
// retains a recognized weekday while leaving its time order unknown.
struct SubPrepNavigationScheduleOrderFact final
{
    std::int32_t weekdayOrder = 0;
    std::optional<std::int32_t> startMinute;

    friend bool operator==(
        const SubPrepNavigationScheduleOrderFact&,
        const SubPrepNavigationScheduleOrderFact&
        ) = default;
};

// Reduces the full candidate schedule to its earliest canonical weekday and
// the earliest valid start minute on that weekday.
[[nodiscard]] inline std::pair<std::int32_t, std::int32_t>
calculateSubPrepNavigationScheduleOrder(
    const std::vector<SubPrepNavigationScheduleOrderFact>& facts
    ) noexcept
{
    std::int32_t firstDayOrder = kClassSummaryUnknownNavigationOrder;
    for (const SubPrepNavigationScheduleOrderFact& fact : facts)
    {
        if (fact.weekdayOrder >= 0 && fact.weekdayOrder <= 6)
        {
            firstDayOrder = std::min(firstDayOrder, fact.weekdayOrder);
        }
    }

    std::int32_t firstTimeOrder = kClassSummaryUnknownNavigationOrder;
    if (firstDayOrder != kClassSummaryUnknownNavigationOrder)
    {
        for (const SubPrepNavigationScheduleOrderFact& fact : facts)
        {
            if (fact.weekdayOrder != firstDayOrder
                || !fact.startMinute
                || *fact.startMinute < 0
                || *fact.startMinute >= 24 * 60)
            {
                continue;
            }
            firstTimeOrder = std::min(firstTimeOrder, *fact.startMinute);
        }
    }

    return {firstDayOrder, firstTimeOrder};
}

} // namespace ClassMngr::Next::Application
