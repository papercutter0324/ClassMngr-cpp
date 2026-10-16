#include "next/application/academic_year_schedule_policy.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <string_view>

using ClassMngr::Next::Application::
    isAcademicYearScheduleValid;
using ClassMngr::Next::Application::
    termWeeksFromDayDistance;

namespace
{
bool termWeeksFromDayDistanceIsValid()
{
    return termWeeksFromDayDistance(7) == 1
        && termWeeksFromDayDistance(371) == 53
        && !termWeeksFromDayDistance(0)
        && !termWeeksFromDayDistance(-7)
        && !termWeeksFromDayDistance(8)
        && !termWeeksFromDayDistance(378)
        && !termWeeksFromDayDistance(7LL * (1LL << 32) + 7);
}

bool academicYearScheduleValidityIsValid()
{
    constexpr std::array<int, 4> boundaryCounts{1, 53, 1, 53};
    constexpr std::array<int, 4> allMaximumCounts{53, 53, 53, 53};
    constexpr std::array<int, 4> zeroCount{1, 0, 1, 1};
    constexpr std::array<int, 4> excessiveCount{1, 54, 1, 1};
    constexpr std::array<int, 3> tooFewCounts{1, 53, 1};
    constexpr std::array<int, 5> tooManyCounts{1, 53, 1, 53, 1};

    return !(
        isAcademicYearScheduleValid(2025, true, boundaryCounts)
        || !isAcademicYearScheduleValid(2026, true, boundaryCounts)
        || isAcademicYearScheduleValid(2026, false, boundaryCounts)
        || !isAcademicYearScheduleValid(2026, true, allMaximumCounts)
        || isAcademicYearScheduleValid(2026, true, zeroCount)
        || isAcademicYearScheduleValid(2026, true, excessiveCount)
        || isAcademicYearScheduleValid(2026, true, tooFewCounts)
        || isAcademicYearScheduleValid(2026, true, tooManyCounts)
    );
}
}

int main(const int argc, char* argv[])
{
    if (argc != 1)
    {
        if (
            argc == 2
            && std::string_view(argv[1]) == "termWeeksFromDayDistance"
            )
        {
            return termWeeksFromDayDistanceIsValid()
                ? EXIT_SUCCESS
                : EXIT_FAILURE;
        }

        return EXIT_FAILURE;
    }

    if (!academicYearScheduleValidityIsValid()
        || !termWeeksFromDayDistanceIsValid())
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
