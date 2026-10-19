#include "next/application/sub_prep_print_order_policy.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace ClassMngr::Next::Application;
using ClassMngr::Next::Domain::ClassId;
using ClassMngr::Next::Domain::TeacherId;

namespace
{

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
               const std::size_t leftIndex,
               const std::size_t rightIndex,
               const SubPrepPrintTeacherNameField field
           )
    {
        const std::vector<std::string>& names =
            field == SubPrepPrintTeacherNameField::English
                ? englishNames
                : koreanNames;
        return compareText(
            names[leftIndex],
            names[rightIndex]
            );
    };
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
        ids.push_back(numericId(teachers[group.teacherIndex].teacherId));
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
    for (const std::size_t index : group.classIndexes)
    {
        ids.push_back(numericId(classes[index].classId));
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
                                const std::size_t leftIndex,
                                const std::size_t rightIndex,
                                const SubPrepPrintTeacherNameField field
                            )
    {
        if (field == SubPrepPrintTeacherNameField::English)
        {
            comparedEnglish = true;
            return compareText(
                englishNames[leftIndex],
                englishNames[rightIndex]
                );
        }

        comparedKorean = true;
        return compareText(
            koreanNames[leftIndex],
            koreanNames[rightIndex]
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
            {teacherKey(1), 1, 9, 6, 1'300, classKey(90)},
            {teacherKey(1), 2, 0, 0, 0, classKey(1)}
        },
        comparator
        );
    const auto level = orderSubPrepPrintGroups(
        teachers,
        {
            {teacherKey(1), 1, 1, 6, 1'300, classKey(90)},
            {teacherKey(1), 1, 2, 0, 0, classKey(1)}
        },
        comparator
        );
    const auto weekday = orderSubPrepPrintGroups(
        teachers,
        {
            {teacherKey(1), 1, 1, 0, 1'300, classKey(90)},
            {teacherKey(1), 1, 1, 1, 0, classKey(1)}
        },
        comparator
        );
    const auto minute = orderSubPrepPrintGroups(
        teachers,
        {
            {teacherKey(1), 1, 1, 0, 500, classKey(90)},
            {teacherKey(1), 1, 1, 0, 600, classKey(1)}
        },
        comparator
        );
    const auto classId = orderSubPrepPrintGroups(
        teachers,
        {
            {teacherKey(1), 1, 1, 0, 500, classKey(10)},
            {teacherKey(1), 1, 1, 0, 500, classKey(2)}
        },
        comparator
        );

    return orderedClassIds(grade.front(), {
               {teacherKey(1), 1, 9, 6, 1'300, classKey(90)},
               {teacherKey(1), 2, 0, 0, 0, classKey(1)}
           }) == std::vector<int>{90, 1}
        && orderedClassIds(level.front(), {
               {teacherKey(1), 1, 1, 6, 1'300, classKey(90)},
               {teacherKey(1), 1, 2, 0, 0, classKey(1)}
           }) == std::vector<int>{90, 1}
        && orderedClassIds(weekday.front(), {
               {teacherKey(1), 1, 1, 0, 1'300, classKey(90)},
               {teacherKey(1), 1, 1, 1, 0, classKey(1)}
           }) == std::vector<int>{90, 1}
        && orderedClassIds(minute.front(), {
               {teacherKey(1), 1, 1, 0, 500, classKey(90)},
               {teacherKey(1), 1, 1, 0, 600, classKey(1)}
           }) == std::vector<int>{90, 1}
        && orderedClassIds(classId.front(), {
               {teacherKey(1), 1, 1, 0, 500, classKey(10)},
               {teacherKey(1), 1, 1, 0, 500, classKey(2)}
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
            {teacherKey(1), 1, 1, 1, 500, classTextKey("2x")},
            {teacherKey(1), 1, 1, 1, 500, classTextKey("10x")}
        };
        const std::vector<std::string> oneEnglishName{"Teacher"};
        const std::vector<std::string> oneKoreanName{""};
        const auto classGroups = orderSubPrepPrintGroups(
            oneTeacher,
            classes,
            compareNames(oneEnglishName, oneKoreanName)
            );

        return teachers[teacherGroups.at(0).teacherIndex].teacherId.value()
                == "10x"
            && teachers[teacherGroups.at(1).teacherIndex].teacherId.value()
                == "2x"
            && classes[classGroups.at(0).classIndexes.at(0)].classId.value()
                == "10x"
            && classes[classGroups.at(0).classIndexes.at(1)].classId.value()
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
        {teacherKey(1), kSubPrepPrintUnknownOrder, 1, 0, 500, classKey(1)},
        {teacherKey(1), 1, kSubPrepPrintUnknownOrder, 0, 500, classKey(2)},
        {teacherKey(1), 1, 1, kSubPrepPrintUnknownOrder, 400, classKey(3)},
        {teacherKey(1), 1, 1, 0, kSubPrepPrintUnknownOrder, classKey(4)},
        {teacherKey(1), 1, 1, 0, 500, classKey(5)}
    };

    const auto groups = orderSubPrepPrintGroups(
        teachers,
        classes,
        compareNames(englishNames, koreanNames)
        );

    return orderedClassIds(groups.front(), classes)
        == std::vector<int>{5, 4, 3, 2, 1};
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
        {teacherKey(20), 2, 0, 0, 500, classKey(8)},
        {teacherKey(10), 1, 0, 0, 600, classKey(4)},
        {teacherKey(30), 1, 0, 1, 300, classKey(7)},
        {teacherKey(10), 1, 0, 0, 500, classKey(2)}
    };
    const std::vector<SubPrepPrintTeacherOrderFact> teachersB{
        {teacherKey(30), false},
        {teacherKey(20), true},
        {teacherKey(10), true}
    };
    const std::vector<std::string> englishB{"", "Bob", "Alice"};
    const std::vector<std::string> koreanB{"C", "B", "A"};
    const std::vector<SubPrepPrintClassOrderFact> classesB{
        {teacherKey(10), 1, 0, 0, 500, classKey(2)},
        {teacherKey(30), 1, 0, 1, 300, classKey(7)},
        {teacherKey(10), 1, 0, 0, 600, classKey(4)},
        {teacherKey(20), 2, 0, 0, 500, classKey(8)}
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
            numericId(teachersA[group.teacherIndex].teacherId),
            orderedClassIds(group, classesA)
            );
    }
    for (const SubPrepPrintGroupOrder& group : groupsB)
    {
        normalizedB.emplace_back(
            numericId(teachersB[group.teacherIndex].teacherId),
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
