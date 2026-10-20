#pragma once

#include "academic_term_week_count.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace ClassMngr::Next::Application
{

inline constexpr int FirstTermYear = 2026;
inline constexpr int MaximumTermWeeks = 53;
inline constexpr std::size_t AcademicYearScheduleTermCount = 4;

// Convert a resolved day span into a permitted term week count.
[[nodiscard]] inline std::optional<AcademicTermWeekCount>
termWeeksFromDayDistance(
    const std::int64_t dayDistance
    ) noexcept
{
    if (dayDistance <= 0 || dayDistance % 7 != 0)
    {
        return std::nullopt;
    }

    const std::int64_t weeks = dayDistance / 7;
    if (weeks > MaximumTermWeeks)
    {
        return std::nullopt;
    }

    return AcademicTermWeekCount{static_cast<int>(weeks)};
}

// Validate resolved schedule facts without depending on Qt date types.
[[nodiscard]] inline bool isAcademicYearScheduleValid(
    const int termYear,
    const bool winterStartIsValidMonday,
    const std::span<const AcademicTermWeekCount> termWeekCounts
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
        [](const AcademicTermWeekCount count)
        {
            return count.value() >= 1
                && count.value() <= MaximumTermWeeks;
        }
        );
}

} // namespace ClassMngr::Next::Application
