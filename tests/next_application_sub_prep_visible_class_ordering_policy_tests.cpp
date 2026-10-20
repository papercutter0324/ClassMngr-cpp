#include "next/application/sub_prep_visible_class_ordering_policy.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next::Application;

namespace
{

using Fact = SubPrepVisibleClassOrderingFact;
using Rank = ClassTabNavigationOrderRank;
using ClassId = ClassMngr::Next::Domain::ClassId;

static_assert(
    std::is_same_v<
        decltype(std::declval<Fact&>().configuredLevelOrder),
        std::int32_t
        >
    );
static_assert(
    std::is_same_v<
        decltype(std::declval<Fact&>().navigationFirstDayOrder),
        Rank
        >
    );
static_assert(
    std::is_same_v<
        decltype(std::declval<Fact&>().navigationFirstTimeOrder),
        Rank
        >
    );
static_assert(Rank{}.rawValue() == 1'000);
static_assert(!std::is_convertible_v<std::int32_t, Rank>);
static_assert(!std::is_convertible_v<Rank, std::int32_t>);

ClassId classId(const std::string_view value)
{
    return ClassId::fromString(value).value();
}

int compareUtf8(
    const std::string_view left,
    const std::string_view right
    )
{
    if (left < right)
    {
        return -1;
    }
    if (left > right)
    {
        return 1;
    }
    return 0;
}

bool configuredLevelRankHasHighestPriority()
{
    const std::vector<Fact> rows{
        {3, Rank{0}, Rank{100}, "A", classId("1")},
        {2, Rank{6}, Rank{1'300}, "Z", classId("9")},
        {1, Rank{6}, Rank{1'300}, "Z", classId("8")}
    };

    return orderSubPrepVisibleClassRows(rows, compareUtf8)
        == std::vector<std::size_t>{2, 1, 0};
}

bool navigationDayPrecedesMinuteLabelAndClassId()
{
    const std::vector<Fact> rows{
        {1, Rank{1}, Rank{0}, "A", classId("1")},
        {1, Rank{0}, Rank{1'300}, "Z", classId("9")}
    };

    return orderSubPrepVisibleClassRows(rows, compareUtf8)
        == std::vector<std::size_t>{1, 0};
}

bool navigationMinutePrecedesLabelAndClassId()
{
    const std::vector<Fact> rows{
        {1, Rank{0}, Rank{600}, "A", classId("1")},
        {1, Rank{0}, Rank{500}, "Z", classId("9")}
    };

    return orderSubPrepVisibleClassRows(rows, compareUtf8)
        == std::vector<std::size_t>{1, 0};
}

bool injectedLabelComparisonPrecedesClassId()
{
    const std::vector<Fact> rows{
        {1, Rank{0}, Rank{500}, "A", classId("1")},
        {1, Rank{0}, Rank{500}, "Z", classId("9")}
    };
    bool comparatorWasCalled = false;
    const auto reverseLabels = [&comparatorWasCalled](
                                   const std::string_view left,
                                   const std::string_view right
                               )
    {
        comparatorWasCalled = true;
        return compareUtf8(right, left);
    };

    const auto rowOrder = orderSubPrepVisibleClassRows(rows, reverseLabels);
    return comparatorWasCalled
        && rowOrder == std::vector<std::size_t>{1, 0};
}

bool classIdBreaksEqualEarlierTupleFields()
{
    const std::vector<Fact> rows{
        {1, Rank{0}, Rank{500}, "Same", classId("2")},
        {1, Rank{0}, Rank{500}, "Same", classId("10")}
    };

    return orderSubPrepVisibleClassRows(rows, compareUtf8)
        == std::vector<std::size_t>{1, 0};
}

bool numericUnknownTimeSentinelPrecedesLargerValidMinute()
{
    const std::vector<Fact> rows{
        {1, Rank{0}, Rank{1'020}, "A", classId("1")},
        {
            1,
            Rank{0},
            Rank{kClassSummaryUnknownNavigationOrder},
            "Z",
            classId("9")
        }
    };

    return orderSubPrepVisibleClassRows(rows, compareUtf8)
        == std::vector<std::size_t>{1, 0};
}

bool validSixteenFortyAndUnknownTimeFallThroughToLabelComparison()
{
    const Fact validSixteenForty{
        .configuredLevelOrder = 1,
        .navigationFirstDayOrder = Rank{0},
        .navigationFirstTimeOrder = Rank{16 * 60 + 40},
        .baseLabel = "A",
        .classId = classId("9")
    };
    const Fact unknownTime{
        .configuredLevelOrder = 1,
        .navigationFirstDayOrder = Rank{0},
        .baseLabel = "Z",
        .classId = classId("1")
    };

    const std::vector<Fact> rows{validSixteenForty, unknownTime};
    return validSixteenForty.navigationFirstTimeOrder
            == unknownTime.navigationFirstTimeOrder
        && orderSubPrepVisibleClassRows(rows, compareUtf8)
            == std::vector<std::size_t>{0, 1};
}

bool exactTupleTiesRetainInputOrder()
{
    const std::vector<Fact> rows{
        {1, Rank{0}, Rank{500}, "Same", classId("same-id")},
        {1, Rank{0}, Rank{500}, "Same", classId("same-id")},
        {1, Rank{0}, Rank{500}, "Same", classId("same-id")}
    };

    return orderSubPrepVisibleClassRows(rows, compareUtf8)
        == std::vector<std::size_t>{0, 1, 2};
}

} // namespace

int main()
{
    if (!configuredLevelRankHasHighestPriority())
    {
        std::cerr << "Configured level rank should have highest priority.\n";
        return EXIT_FAILURE;
    }
    if (!navigationDayPrecedesMinuteLabelAndClassId())
    {
        std::cerr << "Navigation day should precede later tuple fields.\n";
        return EXIT_FAILURE;
    }
    if (!navigationMinutePrecedesLabelAndClassId())
    {
        std::cerr << "Navigation minute should precede label and class ID.\n";
        return EXIT_FAILURE;
    }
    if (!injectedLabelComparisonPrecedesClassId())
    {
        std::cerr << "The injected label comparator should control label order.\n";
        return EXIT_FAILURE;
    }
    if (!classIdBreaksEqualEarlierTupleFields())
    {
        std::cerr << "Class ID should break equal earlier tuple fields.\n";
        return EXIT_FAILURE;
    }
    if (!numericUnknownTimeSentinelPrecedesLargerValidMinute())
    {
        std::cerr << "The numeric unknown-time sentinel should sort literally.\n";
        return EXIT_FAILURE;
    }
    if (!validSixteenFortyAndUnknownTimeFallThroughToLabelComparison())
    {
        std::cerr
            << "Valid 16:40 and unknown time ranks should fall through to labels.\n";
        return EXIT_FAILURE;
    }
    if (!exactTupleTiesRetainInputOrder())
    {
        std::cerr << "Exact tuple ties should preserve input row order.\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
