#include "next/application/class_tab_navigation_ordering_policy.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

using namespace ClassMngr::Next::Application;
using ClassId = ClassMngr::Next::Domain::ClassId;

namespace
{

using Fact = ClassTabNavigationOrderingFact;

ClassId classId(const std::string_view value)
{
    return ClassId::fromString(value).value();
}

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

    const Fact baseline{1, 1, 1, 1, u"Z", classId("9")};
    return less(Fact{0, 9, 9, 9, u"Z", classId("9")}, baseline)
        && less(Fact{1, 0, 9, 9, u"Z", classId("9")}, baseline)
        && less(Fact{1, 1, 0, 9, u"Z", classId("9")}, baseline)
        && less(Fact{1, 1, 1, 0, u"Z", classId("9")}, baseline);
}

bool injectedLabelComparatorControlsLabelOrder()
{
    const Fact left{1, 2, 3, 4, u"Alpha", classId("1")};
    const Fact right{1, 2, 3, 4, u"Zulu", classId("9")};
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
    const Fact largerId{1, 2, 3, 4, u"Same", classId("10")};
    const Fact smallerId{1, 2, 3, 4, u"Same", classId("2")};
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
    const Fact validSixForty{1, 2, 1'000, 1'000, u"16:40", classId("9")};
    const Fact unknownMinute{1, 2, 1'000, 1'000, u"unknown", classId("2")};
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
    const Fact left{1, 2, 3, 4, u"Same", classId("5")};
    const Fact right{1, 2, 3, 4, u"Same", classId("5")};
    return !classTabNavigationFactLessThan(left, right, compareLabels)
        && !classTabNavigationFactLessThan(right, left, compareLabels);
}

bool typedClassIdDefaultsToMinusOne()
{
    return Fact{}.classId.value() == "-1";
}

bool classIdsCompareSignedInt32BoundariesNumerically()
{
    const ClassId minimum = classId("-2147483648");
    const ClassId nextToMinimum = classId("-2147483647");
    const ClassId maximum = classId("2147483647");
    const ClassId previousToMaximum = classId("2147483646");

    return classTabNavigationClassIdLessThan(minimum, nextToMinimum)
        && classTabNavigationClassIdLessThan(
            previousToMaximum,
            maximum
            )
        && !classTabNavigationClassIdLessThan(maximum, minimum);
}

bool classIdAliasesUseLexicalTieBreakAfterNumericEquality()
{
    const ClassId padded = classId("02");
    const ClassId canonical = classId("2");
    return classTabNavigationClassIdLessThan(padded, canonical)
        && !classTabNavigationClassIdLessThan(canonical, padded);
}

bool parseableIdsPrecedeMalformedAndOutOfRangeValues()
{
    const ClassId maximum = classId("2147483647");
    const ClassId aboveMaximum = classId("2147483648");
    const ClassId belowMinimum = classId("-2147483649");
    const ClassId malformed = classId("not-an-id");
    const ClassId partiallyNumeric = classId("2suffix");

    return classTabNavigationClassIdLessThan(maximum, aboveMaximum)
        && classTabNavigationClassIdLessThan(maximum, belowMinimum)
        && classTabNavigationClassIdLessThan(maximum, malformed)
        && classTabNavigationClassIdLessThan(
            classId("10"),
            partiallyNumeric
            );
}

bool malformedAndOutOfRangeIdsUseDeterministicLexicalOrder()
{
    const ClassId malformedA = classId("alpha");
    const ClassId malformedZ = classId("zulu");
    const ClassId outOfRangeA = classId("2147483648");
    const ClassId outOfRangeB = classId("2147483649");

    return classTabNavigationClassIdLessThan(malformedA, malformedZ)
        && classTabNavigationClassIdLessThan(outOfRangeA, outOfRangeB)
        && !classTabNavigationClassIdLessThan(malformedZ, malformedA);
}

bool representativeIdsFormStrictWeakOrdering()
{
    const std::array<Fact, 14> facts{
        Fact{0, 9, 9, 9, u"Zulu", classId("alpha")},
        Fact{1, 0, 9, 9, u"Zulu", classId("alpha")},
        Fact{1, 2, 0, 9, u"Zulu", classId("alpha")},
        Fact{1, 2, 3, 0, u"Zulu", classId("alpha")},
        Fact{1, 2, 3, 4, u"Alpha", classId("2147483648")},
        Fact{1, 2, 3, 4, u"Same", classId("-2147483648")},
        Fact{1, 2, 3, 4, u"Same", classId("-1")},
        Fact{1, 2, 3, 4, u"Same", classId("02")},
        Fact{1, 2, 3, 4, u"Same", classId("2")},
        Fact{1, 2, 3, 4, u"Same", classId("2147483647")},
        Fact{1, 2, 3, 4, u"Same", classId("2147483648")},
        Fact{1, 2, 3, 4, u"Same", classId("alpha")},
        Fact{1, 2, 3, 4, u"Zulu", classId("-2147483648")},
        Fact{1, 2, 1'000, 1'000, u"16:40", classId("9")}
    };
    const auto less = [](const Fact& left, const Fact& right)
    {
        return classTabNavigationFactLessThan(left, right, compareLabels);
    };

    for (std::size_t left = 0; left < facts.size(); ++left)
    {
        if (less(facts[left], facts[left]))
        {
            return false;
        }
        for (std::size_t right = 0; right < facts.size(); ++right)
        {
            if (less(facts[left], facts[right])
                && less(facts[right], facts[left]))
            {
                return false;
            }

            for (std::size_t third = 0; third < facts.size(); ++third)
            {
                if (less(facts[left], facts[right])
                    && less(facts[right], facts[third])
                    && !less(facts[left], facts[third]))
                {
                    return false;
                }

                const bool equivalentLeftRight =
                    !less(facts[left], facts[right])
                    && !less(facts[right], facts[left]);
                const bool equivalentRightThird =
                    !less(facts[right], facts[third])
                    && !less(facts[third], facts[right]);
                if (equivalentLeftRight
                    && equivalentRightThird
                    && (less(facts[left], facts[third])
                        || less(facts[third], facts[left])))
                {
                    return false;
                }
            }
        }
    }
    return true;
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
    if (!typedClassIdDefaultsToMinusOne())
    {
        std::cerr << "The typed class ID default should be -1.\n";
        return EXIT_FAILURE;
    }
    if (!classIdsCompareSignedInt32BoundariesNumerically())
    {
        std::cerr << "Class IDs should order signed int32 boundary values numerically.\n";
        return EXIT_FAILURE;
    }
    if (!classIdAliasesUseLexicalTieBreakAfterNumericEquality())
    {
        std::cerr << "Numerically equal class ID aliases should use lexical order.\n";
        return EXIT_FAILURE;
    }
    if (!parseableIdsPrecedeMalformedAndOutOfRangeValues())
    {
        std::cerr << "Parseable class IDs should precede malformed IDs.\n";
        return EXIT_FAILURE;
    }
    if (!malformedAndOutOfRangeIdsUseDeterministicLexicalOrder())
    {
        std::cerr << "Malformed and out-of-range IDs should use lexical order.\n";
        return EXIT_FAILURE;
    }
    if (!representativeIdsFormStrictWeakOrdering())
    {
        std::cerr << "Representative class ID comparisons violate strict weak ordering.\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
