#include "next/application/testing_classes_ordering_policy.h"

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next::Application;

using TestingClassesRank = TestingClassesOrderingRank;

static_assert(std::is_default_constructible_v<TestingClassesRank>);
static_assert(std::is_constructible_v<TestingClassesRank, std::int32_t>);
static_assert(!std::is_convertible_v<std::int32_t, TestingClassesRank>);
static_assert(!std::is_convertible_v<TestingClassesRank, std::int32_t>);
static_assert(std::is_same_v<
    decltype(TestingClassesOrderingFact{}.gradeRank),
    TestingClassesRank
    >);
static_assert(std::is_same_v<
    decltype(TestingClassesOrderingFact{}.levelRank),
    TestingClassesRank
    >);
static_assert(TestingClassesRank{}.rawValue() == 1'000);
static_assert(std::is_default_constructible_v<TestingClassesOrderedRowIndex>);
static_assert(std::is_constructible_v<TestingClassesOrderedRowIndex, std::size_t>);
static_assert(!std::is_convertible_v<std::size_t, TestingClassesOrderedRowIndex>);
static_assert(!std::is_convertible_v<TestingClassesOrderedRowIndex, std::size_t>);
static_assert(std::is_same_v<
    decltype(orderTestingClassesRows(
        std::declval<const std::vector<TestingClassesOrderingFact>&>(),
        std::declval<const TestingClassesOrderingTextComparator&>()
        )),
    std::vector<TestingClassesOrderedRowIndex>
    >);

namespace
{

using Fact = TestingClassesOrderingFact;

Fact fact(
    const std::int32_t gradeRank,
    const std::int32_t levelRank,
    std::u16string levelText,
    std::u16string className
    )
{
    return {
        .gradeRank = TestingClassesRank{gradeRank},
        .levelRank = TestingClassesRank{levelRank},
        .levelText = std::move(levelText),
        .className = std::move(className)
    };
}

bool rankHasValueSemantics()
{
    constexpr TestingClassesRank defaultRank;
    constexpr TestingClassesRank explicitRank{7};

    return defaultRank.rawValue() == 1'000
        && explicitRank.rawValue() == 7
        && explicitRank == TestingClassesRank{7}
        && explicitRank != TestingClassesRank{8};
}

bool orderedRowIndexHasValueSemantics()
{
    constexpr TestingClassesOrderedRowIndex defaultIndex;
    constexpr TestingClassesOrderedRowIndex explicitIndex{7};

    return defaultIndex.rawValue() == 0
        && explicitIndex.rawValue() == 7
        && explicitIndex == TestingClassesOrderedRowIndex{7}
        && explicitIndex != TestingClassesOrderedRowIndex{8};
}

int compareUtf16(
    const std::u16string_view left,
    const std::u16string_view right
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

int compareUtf16Descending(
    const std::u16string_view left,
    const std::u16string_view right
    )
{
    return compareUtf16(right, left);
}

bool gradeRankPrecedesLevelRankAndText()
{
    const std::vector<Fact> rows{
        fact(1, 0, u"Alpha", u"Alpha"),
        fact(0, 1, u"Zulu", u"Zulu")
    };

    return orderTestingClassesRows(rows, compareUtf16)
        == std::vector<TestingClassesOrderedRowIndex>{
            TestingClassesOrderedRowIndex{1},
            TestingClassesOrderedRowIndex{0}
        };
}

bool levelRankPrecedesLevelAndClassText()
{
    const std::vector<Fact> rows{
        fact(0, 2, u"Alpha", u"Alpha"),
        fact(0, 1, u"Zulu", u"Zulu")
    };

    return orderTestingClassesRows(rows, compareUtf16)
        == std::vector<TestingClassesOrderedRowIndex>{
            TestingClassesOrderedRowIndex{1},
            TestingClassesOrderedRowIndex{0}
        };
}

bool injectedComparatorOrdersLevelText()
{
    const std::vector<Fact> rows{
        fact(0, 1000, u"Alpha", u"Zulu"),
        fact(0, 1000, u"Zulu", u"Alpha")
    };

    return orderTestingClassesRows(rows, compareUtf16Descending)
        == std::vector<TestingClassesOrderedRowIndex>{
            TestingClassesOrderedRowIndex{1},
            TestingClassesOrderedRowIndex{0}
        };
}

bool injectedComparatorOrdersClassNameAfterEqualLevelText()
{
    const std::vector<Fact> rows{
        fact(0, 0, u"Major", u"Alpha"),
        fact(0, 0, u"Major", u"Zulu")
    };

    return orderTestingClassesRows(rows, compareUtf16Descending)
        == std::vector<TestingClassesOrderedRowIndex>{
            TestingClassesOrderedRowIndex{1},
            TestingClassesOrderedRowIndex{0}
        };
}

bool unknownRankSentinelComparesAfterKnownRanks()
{
    const std::vector<Fact> rows{
        fact(1000, 0, u"Alpha", u"Unknown grade"),
        fact(2, 1000, u"Alpha", u"Unknown level"),
        fact(2, 1, u"Zulu", u"Known grade and level")
    };

    return orderTestingClassesRows(rows, compareUtf16)
        == std::vector<TestingClassesOrderedRowIndex>{
            TestingClassesOrderedRowIndex{2},
            TestingClassesOrderedRowIndex{1},
            TestingClassesOrderedRowIndex{0}
        };
}

bool exactTiesHaveNoPromisedOrder()
{
    const std::vector<Fact> rows{
        fact(0, 0, u"Major", u"Same"),
        fact(0, 0, u"Major", u"Same"),
        fact(0, 0, u"Major", u"Same")
    };
    const auto rowOrder = orderTestingClassesRows(rows, compareUtf16);
    if (rowOrder.size() != rows.size())
    {
        return false;
    }

    std::vector<bool> seen(rows.size(), false);
    for (const TestingClassesOrderedRowIndex rowIndex : rowOrder)
    {
        const std::size_t rawIndex = rowIndex.rawValue();
        if (rawIndex >= seen.size() || seen[rawIndex])
        {
            return false;
        }
        seen[rawIndex] = true;
    }
    return true;
}

} // namespace

int main()
{
    if (!rankHasValueSemantics())
    {
        std::cerr << "Testing Classes ranks should preserve their value semantics.\n";
        return EXIT_FAILURE;
    }
    if (!orderedRowIndexHasValueSemantics())
    {
        std::cerr << "Ordered row indexes should have value semantics.\n";
        return EXIT_FAILURE;
    }
    if (!gradeRankPrecedesLevelRankAndText())
    {
        std::cerr << "Grade rank should have highest priority.\n";
        return EXIT_FAILURE;
    }
    if (!levelRankPrecedesLevelAndClassText())
    {
        std::cerr << "Level rank should precede text comparisons.\n";
        return EXIT_FAILURE;
    }
    if (!injectedComparatorOrdersLevelText())
    {
        std::cerr << "The injected comparator should order level text.\n";
        return EXIT_FAILURE;
    }
    if (!injectedComparatorOrdersClassNameAfterEqualLevelText())
    {
        std::cerr << "The injected comparator should order class names.\n";
        return EXIT_FAILURE;
    }
    if (!unknownRankSentinelComparesAfterKnownRanks())
    {
        std::cerr << "Unknown rank 1000 should compare after known ranks.\n";
        return EXIT_FAILURE;
    }
    if (!exactTiesHaveNoPromisedOrder())
    {
        std::cerr << "Exact ties should each remain in the output.\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
