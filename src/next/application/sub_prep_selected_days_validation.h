#pragma once

#include "next/application/sub_prep_weekday.h"

#include <cstddef>
#include <set>
#include <vector>

namespace ClassMngr::Next::Application
{

inline constexpr std::size_t kSubPrepSelectedDaysMaxCount = 7;

enum class SubPrepSelectedDaysValidationStatus
{
    Valid,
    ExceedsWeeklyLimit,
    InvalidWeekday,
    DuplicateWeekday
};

[[nodiscard]] inline SubPrepSelectedDaysValidationStatus
validateSubPrepSelectedDays(
    const std::vector<SubPrepWeekday>& selectedDays
    )
{
    if (selectedDays.size() > kSubPrepSelectedDaysMaxCount)
    {
        return SubPrepSelectedDaysValidationStatus::ExceedsWeeklyLimit;
    }

    std::set<SubPrepWeekday> seenDays;
    for (const auto day : selectedDays)
    {
        if (!isValidSubPrepWeekday(day))
        {
            return SubPrepSelectedDaysValidationStatus::InvalidWeekday;
        }

        if (!seenDays.insert(day).second)
        {
            return SubPrepSelectedDaysValidationStatus::DuplicateWeekday;
        }
    }

    return SubPrepSelectedDaysValidationStatus::Valid;
}

} // namespace ClassMngr::Next::Application
