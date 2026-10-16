#include "next/application/academic_year_schedule_policy.h"

#include <array>
#include <cstdlib>

using ClassMngr::Next::Application::
    isAcademicYearScheduleValid;

int main()
{
    constexpr std::array<int, 4> boundaryCounts{1, 53, 1, 53};
    constexpr std::array<int, 4> allMaximumCounts{53, 53, 53, 53};
    constexpr std::array<int, 4> zeroCount{1, 0, 1, 1};
    constexpr std::array<int, 4> excessiveCount{1, 54, 1, 1};
    constexpr std::array<int, 3> tooFewCounts{1, 53, 1};
    constexpr std::array<int, 5> tooManyCounts{1, 53, 1, 53, 1};

    if (
        isAcademicYearScheduleValid(2025, true, boundaryCounts)
        || !isAcademicYearScheduleValid(2026, true, boundaryCounts)
        || isAcademicYearScheduleValid(2026, false, boundaryCounts)
        || !isAcademicYearScheduleValid(2026, true, allMaximumCounts)
        || isAcademicYearScheduleValid(2026, true, zeroCount)
        || isAcademicYearScheduleValid(2026, true, excessiveCount)
        || isAcademicYearScheduleValid(2026, true, tooFewCounts)
        || isAcademicYearScheduleValid(2026, true, tooManyCounts)
    )
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
