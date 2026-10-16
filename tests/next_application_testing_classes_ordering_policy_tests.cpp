#include "next/application/testing_classes_ordering_policy.h"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

using namespace ClassMngr::Next::Application;

namespace
{

using Fact = TestingClassesOrderingFact;

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
        {1, 0, u"Alpha", u"Alpha"},
        {0, 1, u"Zulu", u"Zulu"}
    };

    return orderTestingClassesRows(rows, compareUtf16)
        == std::vector<std::size_t>{1, 0};
}

bool levelRankPrecedesLevelAndClassText()
{
    const std::vector<Fact> rows{
        {0, 2, u"Alpha", u"Alpha"},
        {0, 1, u"Zulu", u"Zulu"}
    };

    return orderTestingClassesRows(rows, compareUtf16)
        == std::vector<std::size_t>{1, 0};
}

bool injectedComparatorOrdersLevelText()
{
    const std::vector<Fact> rows{
        {0, 1000, u"Alpha", u"Zulu"},
        {0, 1000, u"Zulu", u"Alpha"}
    };

    return orderTestingClassesRows(rows, compareUtf16Descending)
        == std::vector<std::size_t>{1, 0};
}

bool injectedComparatorOrdersClassNameAfterEqualLevelText()
{
    const std::vector<Fact> rows{
        {0, 0, u"Major", u"Alpha"},
        {0, 0, u"Major", u"Zulu"}
    };

    return orderTestingClassesRows(rows, compareUtf16Descending)
        == std::vector<std::size_t>{1, 0};
}

bool unknownRankSentinelComparesAfterKnownRanks()
{
    const std::vector<Fact> rows{
        {1000, 0, u"Alpha", u"Unknown grade"},
        {2, 1000, u"Alpha", u"Unknown level"},
        {2, 1, u"Zulu", u"Known grade and level"}
    };

    return orderTestingClassesRows(rows, compareUtf16)
        == std::vector<std::size_t>{2, 1, 0};
}

bool exactTiesHaveNoPromisedOrder()
{
    const std::vector<Fact> rows{
        {0, 0, u"Major", u"Same"},
        {0, 0, u"Major", u"Same"},
        {0, 0, u"Major", u"Same"}
    };
    const auto rowOrder = orderTestingClassesRows(rows, compareUtf16);
    if (rowOrder.size() != rows.size())
    {
        return false;
    }

    std::vector<bool> seen(rows.size(), false);
    for (const std::size_t rowIndex : rowOrder)
    {
        if (rowIndex >= seen.size() || seen[rowIndex])
        {
            return false;
        }
        seen[rowIndex] = true;
    }
    return true;
}

} // namespace

int main()
{
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
