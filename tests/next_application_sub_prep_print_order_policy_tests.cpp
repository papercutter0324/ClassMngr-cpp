#include "next/application/sub_prep_print_order_policy.h"

#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next::Application;
using ClassMngr::Next::Domain::ClassId;
using ClassMngr::Next::Domain::TeacherId;

namespace
{

static_assert(std::is_constructible_v<SubPrepPrintWeekdayOrder, int>);
static_assert(
    !std::is_convertible_v<int, SubPrepPrintWeekdayOrder>
    );
static_assert(
    !std::is_convertible_v<SubPrepPrintWeekdayOrder, int>
    );
static_assert(
    std::is_constructible_v<SubPrepPrintStartMinute, int>
    );
static_assert(
    !std::is_convertible_v<int, SubPrepPrintStartMinute>
    );
static_assert(
    !std::is_convertible_v<SubPrepPrintStartMinute, int>
    );
static_assert(
    !std::is_same_v<SubPrepPrintWeekdayOrder, SubPrepPrintStartMinute>
    );
static_assert(
    std::is_constructible_v<SubPrepPrintTeacherIndex, std::size_t>
    );
static_assert(
    !std::is_convertible_v<std::size_t, SubPrepPrintTeacherIndex>
    );
static_assert(
    !std::is_convertible_v<SubPrepPrintTeacherIndex, std::size_t>
    );
static_assert(
    std::is_constructible_v<SubPrepPrintClassIndex, std::size_t>
    );
static_assert(
    !std::is_convertible_v<std::size_t, SubPrepPrintClassIndex>
    );
static_assert(
    !std::is_convertible_v<SubPrepPrintClassIndex, std::size_t>
    );
static_assert(
    !std::is_same_v<SubPrepPrintTeacherIndex, SubPrepPrintClassIndex>
    );
static_assert(
    std::is_same_v<
        decltype(SubPrepPrintGroupOrder::teacherIndex),
        SubPrepPrintTeacherIndex
        >
    );
static_assert(
    std::is_same_v<
        decltype(SubPrepPrintGroupOrder::classIndexes),
        std::vector<SubPrepPrintClassIndex>
        >
    );
static_assert(
    std::is_same_v<
        SubPrepPrintTeacherNameComparator,
        std::function<int(
            SubPrepPrintTeacherIndex,
            SubPrepPrintTeacherIndex,
            SubPrepPrintTeacherNameField
            )>
        >
    );

SubPrepPrintWeekdayOrder weekdayOrder(const int value)
{
    return SubPrepPrintWeekdayOrder{value};
}

SubPrepPrintStartMinute startMinute(const int value)
{
    return SubPrepPrintStartMinute{value};
}

bool printOrderIntegerFactsAreStrongValues()
{
    const SubPrepPrintWeekdayOrder defaultWeekday;
    const SubPrepPrintWeekdayOrder weekday{7};
    const SubPrepPrintWeekdayOrder sameWeekday{7};
    const SubPrepPrintWeekdayOrder otherWeekday{8};
    const SubPrepPrintStartMinute defaultMinute;
    const SubPrepPrintStartMinute minute{1'439};
    const SubPrepPrintStartMinute sameMinute{1'439};
    const SubPrepPrintStartMinute otherMinute{1'438};
    const SubPrepPrintClassOrderFact defaultFact;

    return defaultWeekday.value() == 0
        && weekday == sameWeekday
        && !(weekday == otherWeekday)
        && defaultMinute.value() == 0
        && minute == sameMinute
        && !(minute == otherMinute)
        && defaultFact.weekdayOrder.value() == kSubPrepPrintUnknownOrder
        && defaultFact.startMinute.value() == kSubPrepPrintUnknownOrder;
}

bool printOrderIndexesAreStrongValues()
{
    const SubPrepPrintTeacherIndex defaultTeacherIndex;
    const SubPrepPrintTeacherIndex teacherIndex{4};
    const SubPrepPrintTeacherIndex sameTeacherIndex{4};
    const SubPrepPrintTeacherIndex otherTeacherIndex{5};
    const SubPrepPrintClassIndex defaultClassIndex;
    const SubPrepPrintClassIndex classIndex{9};
    const SubPrepPrintClassIndex sameClassIndex{9};
    const SubPrepPrintClassIndex otherClassIndex{10};
    const SubPrepPrintGroupOrder defaultGroup;

    return defaultTeacherIndex.rawValue() == 0
        && teacherIndex.rawValue() == 4
        && teacherIndex == sameTeacherIndex
        && !(teacherIndex == otherTeacherIndex)
        && defaultClassIndex.rawValue() == 0
        && classIndex.rawValue() == 9
        && classIndex == sameClassIndex
        && !(classIndex == otherClassIndex)
        && defaultGroup.teacherIndex.rawValue() == 0
        && defaultGroup.classIndexes.empty();
}

TeacherId teacherKey(const int value)
{
    return *TeacherId::fromString(std::to_string(value));
}

ClassId classKey(const int value)
{
    return *ClassId::fromString(std::to_string(value));
}

TeacherId teacherTextKey(const std::string_view value)
{
    return *TeacherId::fromString(value);
}

ClassId classTextKey(const std::string_view value)
{
    return *ClassId::fromString(value);
}

int numericId(const TeacherId& id)
{
    return std::stoi(id.value());
}

int numericId(const ClassId& id)
{
    return std::stoi(id.value());
}

int compareText(
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

SubPrepPrintTeacherNameComparator compareNames(
    const std::vector<std::string>& englishNames,
    const std::vector<std::string>& koreanNames
    )
{
    return [&englishNames, &koreanNames](
               const SubPrepPrintTeacherIndex leftIndex,
               const SubPrepPrintTeacherIndex rightIndex,
               const SubPrepPrintTeacherNameField field
           )
    {
        const std::vector<std::string>& names =
            field == SubPrepPrintTeacherNameField::English
                ? englishNames
                : koreanNames;
        return compareText(
            names[leftIndex.rawValue()],
            names[rightIndex.rawValue()]
            );
    };
}

bool emptyTeacherInputAndUnmatchedClassesKeepExpectedGroups()
{
    const auto compareEqualNames = [](
                                       const SubPrepPrintTeacherIndex,
                                       const SubPrepPrintTeacherIndex,
                                       const SubPrepPrintTeacherNameField
                                   )
    {
        return 0;
    };
    const auto emptyGroups = orderSubPrepPrintGroups(
        {},
        {},
        compareEqualNames
        );
    const std::vector<SubPrepPrintTeacherOrderFact> teachers{
        {teacherKey(1), true}
    };
    const std::vector<SubPrepPrintClassOrderFact> unmatchedClasses{
        {
            teacherKey(2),
            1,
            1,
            weekdayOrder(1),
            startMinute(500),
            classKey(3)
        }
    };
    const auto unmatchedGroups = orderSubPrepPrintGroups(
        teachers,
        unmatchedClasses,
        compareEqualNames
        );

    return emptyGroups.empty()
        && unmatchedGroups.size() == 1
        && unmatchedGroups.front().teacherIndex.rawValue() == 0
        && unmatchedGroups.front().classIndexes.empty();
}

std::vector<int> orderedTeacherIds(
    const std::vector<SubPrepPrintGroupOrder>& groups,
    const std::vector<SubPrepPrintTeacherOrderFact>& teachers
    )
{
    std::vector<int> ids;
    ids.reserve(groups.size());
    for (const SubPrepPrintGroupOrder& group : groups)
    {
        ids.push_back(
            numericId(teachers[group.teacherIndex.rawValue()].teacherId)
            );
    }
    return ids;
}

std::vector<int> orderedClassIds(
    const SubPrepPrintGroupOrder& group,
    const std::vector<SubPrepPrintClassOrderFact>& classes
    )
{
    std::vector<int> ids;
    ids.reserve(group.classIndexes.size());
    for (const SubPrepPrintClassIndex index : group.classIndexes)
    {
        ids.push_back(numericId(classes[index.rawValue()].classId));
    }
    return ids;
}

bool englishPresenceAndInjectedNameOrderTeachers()
{
    const std::vector<SubPrepPrintTeacherOrderFact> teachers{
        {teacherKey(20), true},
        {teacherKey(10), true},
        {teacherKey(5), false},
        {teacherKey(30), true}
    };
    const std::vector<std::string> englishNames{
        "Zoe",
        "Alice",
        "",
        "Alice"
    };
    const std::vector<std::string> koreanNames{
        "Z",
        "K",
        "A",
        "B"
    };
    bool comparedEnglish = false;
    bool comparedKorean = false;
    const auto comparator = [&] (
                                const SubPrepPrintTeacherIndex leftIndex,
                                const SubPrepPrintTeacherIndex rightIndex,
                                const SubPrepPrintTeacherNameField field
                            )
    {
        if (field == SubPrepPrintTeacherNameField::English)
        {
            comparedEnglish = true;
            return compareText(
                englishNames[leftIndex.rawValue()],
                englishNames[rightIndex.rawValue()]
                );
        }

        comparedKorean = true;
        return compareText(
            koreanNames[leftIndex.rawValue()],
            koreanNames[rightIndex.rawValue()]
            );
    };

    const auto groups = orderSubPrepPrintGroups(
        teachers,
        {},
        comparator
        );

    return comparedEnglish
        && comparedKorean
        && orderedTeacherIds(groups, teachers)
            == std::vector<int>{30, 10, 20, 5};
}

bool teacherIdBreaksCompleteNameTies()
{
    const std::vector<SubPrepPrintTeacherOrderFact> teachers{
        {teacherKey(10), true},
        {teacherKey(2), true},
        {teacherKey(37), false},
        {teacherKey(12), false}
    };
    const std::vector<std::string> englishNames{
        "Same",
        "Same",
        "",
        ""
    };
    const std::vector<std::string> koreanNames{
        "Same",
        "Same",
        "Hangul",
        "Hangul"
    };

    const auto groups = orderSubPrepPrintGroups(
        teachers,
        {},
        compareNames(englishNames, koreanNames)
        );

    return orderedTeacherIds(groups, teachers)
        == std::vector<int>{2, 10, 12, 37};
}

bool eachClassKeyPrecedesTheFollowingKeys()
{
    const std::vector<SubPrepPrintTeacherOrderFact> teachers{{teacherKey(1), true}};
    const std::vector<std::string> englishNames{"Teacher"};
    const std::vector<std::string> koreanNames{""};
    const auto comparator = compareNames(englishNames, koreanNames);

    const auto grade = orderSubPrepPrintGroups(
        teachers,
        {
            {teacherKey(1), 1, 9, weekdayOrder(6), startMinute(1'300), classKey(90)},
            {teacherKey(1), 2, 0, weekdayOrder(0), startMinute(0), classKey(1)}
        },
        comparator
        );
    const auto level = orderSubPrepPrintGroups(
        teachers,
        {
            {teacherKey(1), 1, 1, weekdayOrder(6), startMinute(1'300), classKey(90)},
            {teacherKey(1), 1, 2, weekdayOrder(0), startMinute(0), classKey(1)}
        },
        comparator
        );
    const auto weekday = orderSubPrepPrintGroups(
        teachers,
        {
            {teacherKey(1), 1, 1, weekdayOrder(0), startMinute(1'300), classKey(90)},
            {teacherKey(1), 1, 1, weekdayOrder(1), startMinute(0), classKey(1)}
        },
        comparator
        );
    const auto minute = orderSubPrepPrintGroups(
        teachers,
        {
            {teacherKey(1), 1, 1, weekdayOrder(0), startMinute(500), classKey(90)},
            {teacherKey(1), 1, 1, weekdayOrder(0), startMinute(600), classKey(1)}
        },
        comparator
        );
    const auto classId = orderSubPrepPrintGroups(
        teachers,
        {
            {teacherKey(1), 1, 1, weekdayOrder(0), startMinute(500), classKey(10)},
            {teacherKey(1), 1, 1, weekdayOrder(0), startMinute(500), classKey(2)}
        },
        comparator
        );

    return orderedClassIds(grade.front(), {
               {teacherKey(1), 1, 9, weekdayOrder(6), startMinute(1'300), classKey(90)},
               {teacherKey(1), 2, 0, weekdayOrder(0), startMinute(0), classKey(1)}
           }) == std::vector<int>{90, 1}
        && orderedClassIds(level.front(), {
               {teacherKey(1), 1, 1, weekdayOrder(6), startMinute(1'300), classKey(90)},
               {teacherKey(1), 1, 2, weekdayOrder(0), startMinute(0), classKey(1)}
           }) == std::vector<int>{90, 1}
        && orderedClassIds(weekday.front(), {
               {teacherKey(1), 1, 1, weekdayOrder(0), startMinute(1'300), classKey(90)},
               {teacherKey(1), 1, 1, weekdayOrder(1), startMinute(0), classKey(1)}
           }) == std::vector<int>{90, 1}
        && orderedClassIds(minute.front(), {
               {teacherKey(1), 1, 1, weekdayOrder(0), startMinute(500), classKey(90)},
               {teacherKey(1), 1, 1, weekdayOrder(0), startMinute(600), classKey(1)}
           }) == std::vector<int>{90, 1}
        && orderedClassIds(classId.front(), {
               {teacherKey(1), 1, 1, weekdayOrder(0), startMinute(500), classKey(10)},
               {teacherKey(1), 1, 1, weekdayOrder(0), startMinute(500), classKey(2)}
           }) == std::vector<int>{2, 10};
}

bool malformedTypedIdsUseDeterministicTieBreaks()
{
    try
    {
        const std::vector<SubPrepPrintTeacherOrderFact> teachers{
            {teacherTextKey("2x"), true},
            {teacherTextKey("10x"), true}
        };
        const std::vector<std::string> englishNames{"Same", "Same"};
        const std::vector<std::string> koreanNames{"Same", "Same"};
        const auto teacherGroups = orderSubPrepPrintGroups(
            teachers,
            {},
            compareNames(englishNames, koreanNames)
            );

        const std::vector<SubPrepPrintTeacherOrderFact> oneTeacher{
            {teacherKey(1), true}
        };
        const std::vector<SubPrepPrintClassOrderFact> classes{
            {teacherKey(1), 1, 1, weekdayOrder(1), startMinute(500), classTextKey("2x")},
            {teacherKey(1), 1, 1, weekdayOrder(1), startMinute(500), classTextKey("10x")}
        };
        const std::vector<std::string> oneEnglishName{"Teacher"};
        const std::vector<std::string> oneKoreanName{""};
        const auto classGroups = orderSubPrepPrintGroups(
            oneTeacher,
            classes,
            compareNames(oneEnglishName, oneKoreanName)
            );

        return teachers[
                   teacherGroups.at(0).teacherIndex.rawValue()
                   ].teacherId.value()
                == "10x"
            && teachers[
                   teacherGroups.at(1).teacherIndex.rawValue()
                   ].teacherId.value()
                == "2x"
            && classes[
                   classGroups.at(0).classIndexes.at(0).rawValue()
                   ].classId.value()
                == "10x"
            && classes[
                   classGroups.at(0).classIndexes.at(1).rawValue()
                   ].classId.value()
                == "2x";
    }
    catch (...)
    {
        return false;
    }
}

bool unknownRanksAndMeetingsKeepTheNumericSentinelOrder()
{
    const std::vector<SubPrepPrintTeacherOrderFact> teachers{{teacherKey(1), true}};
    const std::vector<std::string> englishNames{"Teacher"};
    const std::vector<std::string> koreanNames{""};
    const std::vector<SubPrepPrintClassOrderFact> classes{
        {
            teacherKey(1),
            kSubPrepPrintUnknownOrder,
            1,
            weekdayOrder(0),
            startMinute(500),
            classKey(1)
        },
        {
            teacherKey(1),
            1,
            kSubPrepPrintUnknownOrder,
            weekdayOrder(0),
            startMinute(500),
            classKey(2)
        },
        {
            teacherKey(1),
            1,
            1,
            weekdayOrder(kSubPrepPrintUnknownOrder),
            startMinute(400),
            classKey(3)
        },
        {
            teacherKey(1),
            1,
            1,
            weekdayOrder(0),
            startMinute(kSubPrepPrintUnknownOrder),
            classKey(4)
        },
        {
            teacherKey(1),
            1,
            1,
            weekdayOrder(0),
            startMinute(500),
            classKey(5)
        },
        {
            teacherKey(1),
            1,
            1,
            weekdayOrder(kSubPrepPrintUnknownOrder),
            startMinute(500),
            classKey(6)
        },
        {
            teacherKey(1),
            1,
            1,
            weekdayOrder(kSubPrepPrintUnknownOrder),
            startMinute(kSubPrepPrintUnknownOrder),
            classKey(7)
        }
    };

    const auto groups = orderSubPrepPrintGroups(
        teachers,
        classes,
        compareNames(englishNames, koreanNames)
        );

    return orderedClassIds(groups.front(), classes)
        == std::vector<int>{5, 4, 3, 6, 7, 2, 1};
}

bool validUniqueIdsMakeOrderIndependentOfInputOrder()
{
    const std::vector<SubPrepPrintTeacherOrderFact> teachersA{
        {teacherKey(20), true},
        {teacherKey(10), true},
        {teacherKey(30), false}
    };
    const std::vector<std::string> englishA{"Bob", "Alice", ""};
    const std::vector<std::string> koreanA{"B", "A", "C"};
    const std::vector<SubPrepPrintClassOrderFact> classesA{
        {teacherKey(20), 2, 0, weekdayOrder(0), startMinute(500), classKey(8)},
        {teacherKey(10), 1, 0, weekdayOrder(0), startMinute(600), classKey(4)},
        {teacherKey(30), 1, 0, weekdayOrder(1), startMinute(300), classKey(7)},
        {teacherKey(10), 1, 0, weekdayOrder(0), startMinute(500), classKey(2)}
    };
    const std::vector<SubPrepPrintTeacherOrderFact> teachersB{
        {teacherKey(30), false},
        {teacherKey(20), true},
        {teacherKey(10), true}
    };
    const std::vector<std::string> englishB{"", "Bob", "Alice"};
    const std::vector<std::string> koreanB{"C", "B", "A"};
    const std::vector<SubPrepPrintClassOrderFact> classesB{
        {teacherKey(10), 1, 0, weekdayOrder(0), startMinute(500), classKey(2)},
        {teacherKey(30), 1, 0, weekdayOrder(1), startMinute(300), classKey(7)},
        {teacherKey(10), 1, 0, weekdayOrder(0), startMinute(600), classKey(4)},
        {teacherKey(20), 2, 0, weekdayOrder(0), startMinute(500), classKey(8)}
    };

    const auto groupsA = orderSubPrepPrintGroups(
        teachersA,
        classesA,
        compareNames(englishA, koreanA)
        );
    const auto groupsB = orderSubPrepPrintGroups(
        teachersB,
        classesB,
        compareNames(englishB, koreanB)
        );

    std::vector<std::pair<int, std::vector<int>>> normalizedA;
    std::vector<std::pair<int, std::vector<int>>> normalizedB;
    for (const SubPrepPrintGroupOrder& group : groupsA)
    {
        normalizedA.emplace_back(
            numericId(teachersA[group.teacherIndex.rawValue()].teacherId),
            orderedClassIds(group, classesA)
            );
    }
    for (const SubPrepPrintGroupOrder& group : groupsB)
    {
        normalizedB.emplace_back(
            numericId(teachersB[group.teacherIndex.rawValue()].teacherId),
            orderedClassIds(group, classesB)
            );
    }

    return normalizedA == normalizedB
        && normalizedA
            == std::vector<std::pair<int, std::vector<int>>>{
                {10, {2, 4}},
                {20, {8}},
                {30, {7}}
            };
}

} // namespace

int main()
{
    if (!emptyTeacherInputAndUnmatchedClassesKeepExpectedGroups())
    {
        std::cerr << "Empty teacher input and unmatched classes should preserve expected groups.\n";
        return EXIT_FAILURE;
    }
    if (!printOrderIndexesAreStrongValues())
    {
        std::cerr << "Print order indexes should retain distinct strong values.\n";
        return EXIT_FAILURE;
    }
    if (!printOrderIntegerFactsAreStrongValues())
    {
        std::cerr << "Print order integer facts should retain exact strong values.\n";
        return EXIT_FAILURE;
    }
    if (!englishPresenceAndInjectedNameOrderTeachers())
    {
        std::cerr << "Teacher ordering should use English presence and injected name comparison.\n";
        return EXIT_FAILURE;
    }
    if (!teacherIdBreaksCompleteNameTies())
    {
        std::cerr << "Teacher IDs should break complete name ties.\n";
        return EXIT_FAILURE;
    }
    if (!eachClassKeyPrecedesTheFollowingKeys())
    {
        std::cerr << "Class order keys should take priority in configured order.\n";
        return EXIT_FAILURE;
    }
    if (!malformedTypedIdsUseDeterministicTieBreaks())
    {
        std::cerr << "Malformed typed IDs should use deterministic, nonthrowing tie-breaks.\n";
        return EXIT_FAILURE;
    }
    if (!unknownRanksAndMeetingsKeepTheNumericSentinelOrder())
    {
        std::cerr << "Unknown class order keys should retain the numeric sentinel order.\n";
        return EXIT_FAILURE;
    }
    if (!validUniqueIdsMakeOrderIndependentOfInputOrder())
    {
        std::cerr << "Valid unique IDs should determine order independently of input order.\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
