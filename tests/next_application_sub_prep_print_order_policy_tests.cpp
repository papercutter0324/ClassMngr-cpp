#include "next/application/sub_prep_print_order_policy.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace ClassMngr::Next::Application;

namespace
{

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
        ids.push_back(teachers[group.teacherIndex].teacherId);
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
        ids.push_back(classes[index].classId);
    }
    return ids;
}

bool englishPresenceAndInjectedNameOrderTeachers()
{
    const std::vector<SubPrepPrintTeacherOrderFact> teachers{
        {20, true},
        {10, true},
        {5, false},
        {30, true}
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
        {90, true},
        {4, true},
        {37, false},
        {12, false}
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
        == std::vector<int>{4, 90, 12, 37};
}

bool eachClassKeyPrecedesTheFollowingKeys()
{
    const std::vector<SubPrepPrintTeacherOrderFact> teachers{{1, true}};
    const std::vector<std::string> englishNames{"Teacher"};
    const std::vector<std::string> koreanNames{""};
    const auto comparator = compareNames(englishNames, koreanNames);

    const auto grade = orderSubPrepPrintGroups(
        teachers,
        {
            {1, 1, 9, 6, 1'300, 90},
            {1, 2, 0, 0, 0, 1}
        },
        comparator
        );
    const auto level = orderSubPrepPrintGroups(
        teachers,
        {
            {1, 1, 1, 6, 1'300, 90},
            {1, 1, 2, 0, 0, 1}
        },
        comparator
        );
    const auto weekday = orderSubPrepPrintGroups(
        teachers,
        {
            {1, 1, 1, 0, 1'300, 90},
            {1, 1, 1, 1, 0, 1}
        },
        comparator
        );
    const auto minute = orderSubPrepPrintGroups(
        teachers,
        {
            {1, 1, 1, 0, 500, 90},
            {1, 1, 1, 0, 600, 1}
        },
        comparator
        );
    const auto classId = orderSubPrepPrintGroups(
        teachers,
        {
            {1, 1, 1, 0, 500, 90},
            {1, 1, 1, 0, 500, 1}
        },
        comparator
        );

    return orderedClassIds(grade.front(), {
               {1, 1, 9, 6, 1'300, 90},
               {1, 2, 0, 0, 0, 1}
           }) == std::vector<int>{90, 1}
        && orderedClassIds(level.front(), {
               {1, 1, 1, 6, 1'300, 90},
               {1, 1, 2, 0, 0, 1}
           }) == std::vector<int>{90, 1}
        && orderedClassIds(weekday.front(), {
               {1, 1, 1, 0, 1'300, 90},
               {1, 1, 1, 1, 0, 1}
           }) == std::vector<int>{90, 1}
        && orderedClassIds(minute.front(), {
               {1, 1, 1, 0, 500, 90},
               {1, 1, 1, 0, 600, 1}
           }) == std::vector<int>{90, 1}
        && orderedClassIds(classId.front(), {
               {1, 1, 1, 0, 500, 90},
               {1, 1, 1, 0, 500, 1}
           }) == std::vector<int>{1, 90};
}

bool unknownRanksAndMeetingsKeepTheNumericSentinelOrder()
{
    const std::vector<SubPrepPrintTeacherOrderFact> teachers{{1, true}};
    const std::vector<std::string> englishNames{"Teacher"};
    const std::vector<std::string> koreanNames{""};
    const std::vector<SubPrepPrintClassOrderFact> classes{
        {1, kSubPrepPrintUnknownOrder, 1, 0, 500, 1},
        {1, 1, kSubPrepPrintUnknownOrder, 0, 500, 2},
        {1, 1, 1, kSubPrepPrintUnknownOrder, 400, 3},
        {1, 1, 1, 0, kSubPrepPrintUnknownOrder, 4},
        {1, 1, 1, 0, 500, 5}
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
        {20, true},
        {10, true},
        {30, false}
    };
    const std::vector<std::string> englishA{"Bob", "Alice", ""};
    const std::vector<std::string> koreanA{"B", "A", "C"};
    const std::vector<SubPrepPrintClassOrderFact> classesA{
        {20, 2, 0, 0, 500, 8},
        {10, 1, 0, 0, 600, 4},
        {30, 1, 0, 1, 300, 7},
        {10, 1, 0, 0, 500, 2}
    };
    const std::vector<SubPrepPrintTeacherOrderFact> teachersB{
        {30, false},
        {20, true},
        {10, true}
    };
    const std::vector<std::string> englishB{"", "Bob", "Alice"};
    const std::vector<std::string> koreanB{"C", "B", "A"};
    const std::vector<SubPrepPrintClassOrderFact> classesB{
        {10, 1, 0, 0, 500, 2},
        {30, 1, 0, 1, 300, 7},
        {10, 1, 0, 0, 600, 4},
        {20, 2, 0, 0, 500, 8}
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
            teachersA[group.teacherIndex].teacherId,
            orderedClassIds(group, classesA)
            );
    }
    for (const SubPrepPrintGroupOrder& group : groupsB)
    {
        normalizedB.emplace_back(
            teachersB[group.teacherIndex].teacherId,
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
