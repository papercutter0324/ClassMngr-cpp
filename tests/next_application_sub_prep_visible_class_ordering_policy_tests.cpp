#include "next/application/sub_prep_visible_class_ordering_policy.h"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

using namespace ClassMngr::Next::Application;

namespace
{

using Fact = SubPrepVisibleClassOrderingFact;

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
        {3, 0, 100, "A", "1"},
        {2, 6, 1'300, "Z", "9"},
        {1, 6, 1'300, "Z", "8"}
    };

    return orderSubPrepVisibleClassRows(rows, compareUtf8)
        == std::vector<std::size_t>{2, 1, 0};
}

bool navigationDayPrecedesMinuteLabelAndClassId()
{
    const std::vector<Fact> rows{
        {1, 1, 0, "A", "1"},
        {1, 0, 1'300, "Z", "9"}
    };

    return orderSubPrepVisibleClassRows(rows, compareUtf8)
        == std::vector<std::size_t>{1, 0};
}

bool navigationMinutePrecedesLabelAndClassId()
{
    const std::vector<Fact> rows{
        {1, 0, 600, "A", "1"},
        {1, 0, 500, "Z", "9"}
    };

    return orderSubPrepVisibleClassRows(rows, compareUtf8)
        == std::vector<std::size_t>{1, 0};
}

bool injectedLabelComparisonPrecedesClassId()
{
    const std::vector<Fact> rows{
        {1, 0, 500, "A", "1"},
        {1, 0, 500, "Z", "9"}
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
        {1, 0, 500, "Same", "2"},
        {1, 0, 500, "Same", "1"}
    };

    return orderSubPrepVisibleClassRows(rows, compareUtf8)
        == std::vector<std::size_t>{1, 0};
}

bool numericUnknownTimeSentinelPrecedesLargerValidMinute()
{
    const std::vector<Fact> rows{
        {1, 0, 1'020, "A", "1"},
        {1, 0, kClassSummaryUnknownNavigationOrder, "Z", "9"}
    };

    return orderSubPrepVisibleClassRows(rows, compareUtf8)
        == std::vector<std::size_t>{1, 0};
}

bool exactTupleTiesRetainInputOrder()
{
    const std::vector<Fact> rows{
        {1, 0, 500, "Same", "same-id"},
        {1, 0, 500, "Same", "same-id"},
        {1, 0, 500, "Same", "same-id"}
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
    if (!exactTupleTiesRetainInputOrder())
    {
        std::cerr << "Exact tuple ties should preserve input row order.\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
