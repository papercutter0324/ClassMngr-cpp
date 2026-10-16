#include "next/application/class_tab_navigation_ordering_policy.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

using namespace ClassMngr::Next::Application;

namespace
{

using Fact = ClassTabNavigationOrderingFact;

int compareLabels(
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

bool tupleFieldsCompareInPriorityOrder()
{
    const auto less = [](const Fact& left, const Fact& right)
    {
        return classTabNavigationFactLessThan(left, right, compareLabels);
    };

    const Fact baseline{1, 1, 1, 1, u"Z", 9};
    return less(Fact{0, 9, 9, 9, u"Z", 9}, baseline)
        && less(Fact{1, 0, 9, 9, u"Z", 9}, baseline)
        && less(Fact{1, 1, 0, 9, u"Z", 9}, baseline)
        && less(Fact{1, 1, 1, 0, u"Z", 9}, baseline);
}

bool injectedLabelComparatorControlsLabelOrder()
{
    const Fact left{1, 2, 3, 4, u"Alpha", 1};
    const Fact right{1, 2, 3, 4, u"Zulu", 9};
    bool comparatorWasCalled = false;
    const auto reverseLabels = [&comparatorWasCalled](
                                  const std::u16string_view leftLabel,
                                  const std::u16string_view rightLabel
                              )
    {
        comparatorWasCalled = true;
        return compareLabels(rightLabel, leftLabel);
    };

    const bool leftPrecedes =
        classTabNavigationFactLessThan(left, right, reverseLabels);
    const bool rightPrecedes =
        classTabNavigationFactLessThan(right, left, reverseLabels);
    return !leftPrecedes && rightPrecedes && comparatorWasCalled;
}

bool classIdIsNumericFinalTieBreaker()
{
    const Fact largerId{1, 2, 3, 4, u"Same", 10};
    const Fact smallerId{1, 2, 3, 4, u"Same", 2};
    return classTabNavigationFactLessThan(
               smallerId,
               largerId,
               compareLabels
               )
        && !classTabNavigationFactLessThan(
            largerId,
            smallerId,
            compareLabels
            );
}

bool literalSentinelCollisionFallsThroughToLabelComparator()
{
    const Fact validSixForty{1, 2, 1'000, 1'000, u"16:40", 9};
    const Fact unknownMinute{1, 2, 1'000, 1'000, u"unknown", 2};
    return classTabNavigationFactLessThan(
               validSixForty,
               unknownMinute,
               compareLabels
               )
        && !classTabNavigationFactLessThan(
            unknownMinute,
            validSixForty,
            compareLabels
            );
}

bool identicalFactsCompareFalseInBothDirections()
{
    const Fact left{1, 2, 3, 4, u"Same", 5};
    const Fact right{1, 2, 3, 4, u"Same", 5};
    return !classTabNavigationFactLessThan(left, right, compareLabels)
        && !classTabNavigationFactLessThan(right, left, compareLabels);
}

} // namespace

int main()
{
    if (!tupleFieldsCompareInPriorityOrder())
    {
        std::cerr << "Class tab ordering tuple priorities are incorrect.\n";
        return EXIT_FAILURE;
    }
    if (!injectedLabelComparatorControlsLabelOrder())
    {
        std::cerr << "The injected class tab label comparator was ignored.\n";
        return EXIT_FAILURE;
    }
    if (!classIdIsNumericFinalTieBreaker())
    {
        std::cerr << "Class ID should be the numeric final tie breaker.\n";
        return EXIT_FAILURE;
    }
    if (!literalSentinelCollisionFallsThroughToLabelComparator())
    {
        std::cerr << "Literal sentinel collisions should compare by label.\n";
        return EXIT_FAILURE;
    }
    if (!identicalFactsCompareFalseInBothDirections())
    {
        std::cerr << "Identical facts should compare false in both directions.\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
