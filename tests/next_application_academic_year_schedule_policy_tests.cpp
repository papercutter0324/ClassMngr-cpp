#include "next/application/academic_year_schedule_policy.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>

using ClassMngr::Next::Application::AcademicTermYear;
using ClassMngr::Next::Application::AcademicTermWeekCount;
using ClassMngr::Next::Application::
    isAcademicYearScheduleValid;
using ClassMngr::Next::Application::
    termWeeksFromDayDistance;

namespace
{
bool academicTermYearIsValid()
{
    constexpr AcademicTermYear defaultYear;
    constexpr AcademicTermYear firstTermYear{2026};
    constexpr AcademicTermYear anotherFirstTermYear{2026};
    constexpr AcademicTermYear previousYear{2025};

    static_assert(std::is_constructible_v<AcademicTermYear, int>);
    static_assert(
        std::is_same_v<decltype(defaultYear.value()), int>
        );
    static_assert(!std::is_convertible_v<int, AcademicTermYear>);
    static_assert(!std::is_convertible_v<AcademicTermYear, int>);
    static_assert(std::is_same_v<
        decltype(&isAcademicYearScheduleValid),
        bool (*)(
            AcademicTermYear,
            bool,
            std::span<const AcademicTermWeekCount>
            ) noexcept
        >);

    return defaultYear.value() == 0
        && firstTermYear.value() == 2026
        && firstTermYear == anotherFirstTermYear
        && firstTermYear != previousYear;
}

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

    constexpr AcademicTermYear firstTermYear{2026};
    constexpr AcademicTermYear previousYear{2025};
    constexpr AcademicTermYear maximumYear{std::numeric_limits<int>::max()};

    return !isAcademicYearScheduleValid(previousYear, true, boundaryCounts)
        && isAcademicYearScheduleValid(firstTermYear, true, boundaryCounts)
        && isAcademicYearScheduleValid(maximumYear, true, boundaryCounts)
        && !isAcademicYearScheduleValid(firstTermYear, false, boundaryCounts)
        && isAcademicYearScheduleValid(firstTermYear, true, allMaximumCounts)
        && !isAcademicYearScheduleValid(firstTermYear, true, zeroCount)
        && !isAcademicYearScheduleValid(firstTermYear, true, excessiveCount)
        && !isAcademicYearScheduleValid(firstTermYear, true, tooFewCounts)
        && !isAcademicYearScheduleValid(firstTermYear, true, tooManyCounts);
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
        || !academicTermYearIsValid()
        || !academicTermWeekCountIsValid()
        || !termWeeksFromDayDistanceIsValid())
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
