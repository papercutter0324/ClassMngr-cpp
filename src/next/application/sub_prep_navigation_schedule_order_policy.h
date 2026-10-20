#pragma once

#include "next/application/class_summary_projection.h"
#include "next/domain/schedule_time.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

class SubPrepNavigationStartMinute final
{
public:
    constexpr SubPrepNavigationStartMinute() noexcept = default;

    explicit constexpr SubPrepNavigationStartMinute(
        const std::int32_t value
        ) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr std::int32_t value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const SubPrepNavigationStartMinute&,
        const SubPrepNavigationStartMinute&
        ) = default;

private:
    std::int32_t m_value = 0;
};

// A schedule-order fact has already passed Platform's exact weekday and raw
// time parsing. Weekday order is Monday=0 through Sunday=6; an absent minute
// retains a recognized weekday while leaving its time order unknown.
struct SubPrepNavigationScheduleOrderFact final
{
    Domain::Weekday weekdayOrder = Domain::Weekday::Monday;
    std::optional<SubPrepNavigationStartMinute> startMinute;

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
        const std::int32_t weekdayOrder =
            static_cast<std::int32_t>(fact.weekdayOrder);
        if (weekdayOrder >= 0 && weekdayOrder <= 6)
        {
            firstDayOrder = std::min(firstDayOrder, weekdayOrder);
        }
    }

    std::optional<std::int32_t> firstTimeOrder;
    if (firstDayOrder != kClassSummaryUnknownNavigationOrder)
    {
        for (const SubPrepNavigationScheduleOrderFact& fact : facts)
        {
            const std::int32_t weekdayOrder =
                static_cast<std::int32_t>(fact.weekdayOrder);
            if (weekdayOrder != firstDayOrder || !fact.startMinute)
            {
                continue;
            }
            const std::int32_t startMinute = fact.startMinute->value();
            if (startMinute < 0 || startMinute >= 24 * 60)
            {
                continue;
            }
            firstTimeOrder = firstTimeOrder
                ? std::min(*firstTimeOrder, startMinute)
                : startMinute;
        }
    }

    return {
        firstDayOrder,
        firstTimeOrder.value_or(kClassSummaryUnknownNavigationOrder)
    };
}

} // namespace ClassMngr::Next::Application
