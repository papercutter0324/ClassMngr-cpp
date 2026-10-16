#pragma once

#include <algorithm>
#include <cstddef>
#include <span>

namespace ClassMngr::Next::Application
{

inline constexpr int FirstTermYear = 2026;
inline constexpr int MaximumTermWeeks = 53;
inline constexpr std::size_t AcademicYearScheduleTermCount = 4;

// Validate resolved schedule facts without depending on Qt date types.
[[nodiscard]] inline bool isAcademicYearScheduleValid(
    const int termYear,
    const bool winterStartIsValidMonday,
    const std::span<const int> termWeekCounts
    ) noexcept
{
    if (
        termYear < FirstTermYear
        || !winterStartIsValidMonday
        || termWeekCounts.size() != AcademicYearScheduleTermCount
        )
    {
        return false;
    }

    return std::ranges::all_of(
        termWeekCounts,
        [](const int count)
        {
            return count >= 1 && count <= MaximumTermWeeks;
        }
        );
}

} // namespace ClassMngr::Next::Application
