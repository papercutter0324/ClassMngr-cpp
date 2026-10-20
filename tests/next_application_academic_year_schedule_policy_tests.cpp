#include "next/application/academic_year_schedule_policy.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <type_traits>

using ClassMngr::Next::Application::AcademicTermWeekCount;
using ClassMngr::Next::Application::
    isAcademicYearScheduleValid;
using ClassMngr::Next::Application::
    termWeeksFromDayDistance;

namespace
{
bool academicTermWeekCountIsValid()
{
    constexpr AcademicTermWeekCount defaultCount;
    constexpr AcademicTermWeekCount oneWeek{1};
    constexpr AcademicTermWeekCount anotherOneWeek{1};
    constexpr AcademicTermWeekCount fiftyThreeWeeks{53};
    constexpr AcademicTermWeekCount zeroWeeks{0};
    constexpr AcademicTermWeekCount fiftyFourWeeks{54};

    static_assert(std::is_constructible_v<AcademicTermWeekCount, int>);
    static_assert(
        std::is_same_v<decltype(defaultCount.value()), int>
        );
    static_assert(!std::is_convertible_v<int, AcademicTermWeekCount>);
    static_assert(!std::is_convertible_v<AcademicTermWeekCount, int>);

    return defaultCount.value() == 0
        && oneWeek.value() == 1
        && fiftyThreeWeeks.value() == 53
        && oneWeek == anotherOneWeek
        && oneWeek != fiftyThreeWeeks
        && zeroWeeks.value() == 0
        && fiftyFourWeeks.value() == 54;
}

bool termWeeksFromDayDistanceIsValid()
{
    const auto oneWeek = termWeeksFromDayDistance(7);
    const auto fiftyThreeWeeks = termWeeksFromDayDistance(371);

    return oneWeek && oneWeek->value() == 1
        && fiftyThreeWeeks && fiftyThreeWeeks->value() == 53
        && !termWeeksFromDayDistance(0)
        && !termWeeksFromDayDistance(-7)
        && !termWeeksFromDayDistance(8)
        && !termWeeksFromDayDistance(378)
        && !termWeeksFromDayDistance(7LL * (1LL << 32) + 7);
}

bool academicYearScheduleValidityIsValid()
{
    constexpr std::array<AcademicTermWeekCount, 4> boundaryCounts{
        AcademicTermWeekCount{1},
        AcademicTermWeekCount{53},
        AcademicTermWeekCount{1},
        AcademicTermWeekCount{53}
    };
    constexpr std::array<AcademicTermWeekCount, 4> allMaximumCounts{
        AcademicTermWeekCount{53},
        AcademicTermWeekCount{53},
        AcademicTermWeekCount{53},
        AcademicTermWeekCount{53}
    };
    constexpr std::array<AcademicTermWeekCount, 4> zeroCount{
        AcademicTermWeekCount{1},
        AcademicTermWeekCount{0},
        AcademicTermWeekCount{1},
        AcademicTermWeekCount{1}
    };
    constexpr std::array<AcademicTermWeekCount, 4> excessiveCount{
        AcademicTermWeekCount{1},
        AcademicTermWeekCount{54},
        AcademicTermWeekCount{1},
        AcademicTermWeekCount{1}
    };
    constexpr std::array<AcademicTermWeekCount, 3> tooFewCounts{
        AcademicTermWeekCount{1},
        AcademicTermWeekCount{53},
        AcademicTermWeekCount{1}
    };
    constexpr std::array<AcademicTermWeekCount, 5> tooManyCounts{
        AcademicTermWeekCount{1},
        AcademicTermWeekCount{53},
        AcademicTermWeekCount{1},
        AcademicTermWeekCount{53},
        AcademicTermWeekCount{1}
    };

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
        || !academicTermWeekCountIsValid()
        || !termWeeksFromDayDistanceIsValid())
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
