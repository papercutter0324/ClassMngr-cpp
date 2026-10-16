#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <unordered_map>
#include <vector>

namespace ClassMngr::Next::Application
{

inline constexpr int kSubPrepPrintUnknownOrder =
    std::numeric_limits<int>::max();

enum class SubPrepPrintTeacherNameField
{
    English,
    Korean
};

// These facts contain ordering keys only. The caller keeps teacher display
// text and supplies its locale-aware comparison through the callback.
struct SubPrepPrintTeacherOrderFact final
{
    int teacherId = 0;
    bool hasEnglishName = false;
};

struct SubPrepPrintClassOrderFact final
{
    int teacherId = 0;
    int gradeOrder = kSubPrepPrintUnknownOrder;
    int levelOrder = kSubPrepPrintUnknownOrder;
    int weekdayOrder = kSubPrepPrintUnknownOrder;
    int startMinute = kSubPrepPrintUnknownOrder;
    int classId = 0;
};

using SubPrepPrintTeacherNameComparator = std::function<int(
    std::size_t,
    std::size_t,
    SubPrepPrintTeacherNameField
    )>;

struct SubPrepPrintGroupOrder final
{
    std::size_t teacherIndex = 0;
    std::vector<std::size_t> classIndexes;

    friend bool operator==(
        const SubPrepPrintGroupOrder&,
        const SubPrepPrintGroupOrder&
        ) = default;
};

// compareTeacherNames must compare the caller's prepared, trimmed names and
// return a negative, zero, or positive result. Returns teacher fact indexes
// and, for each teacher, ordered class fact indexes. Teacher names remain
// caller-owned; indexes let the injected comparator inspect those names
// without copying display text into this policy.
[[nodiscard]] inline std::vector<SubPrepPrintGroupOrder>
orderSubPrepPrintGroups(
    const std::vector<SubPrepPrintTeacherOrderFact>& teachers,
    const std::vector<SubPrepPrintClassOrderFact>& classes,
    const SubPrepPrintTeacherNameComparator& compareTeacherNames
    )
{
    std::unordered_map<int, std::size_t> teacherIndexesById;
    teacherIndexesById.reserve(teachers.size());
    for (std::size_t index = 0; index < teachers.size(); ++index)
    {
        teacherIndexesById.emplace(teachers[index].teacherId, index);
    }

    std::vector<SubPrepPrintGroupOrder> groups;
    groups.reserve(teachers.size());
    for (std::size_t index = 0; index < teachers.size(); ++index)
    {
        groups.push_back({index, {}});
    }

    for (std::size_t classIndex = 0; classIndex < classes.size(); ++classIndex)
    {
        const auto teacher = teacherIndexesById.find(
            classes[classIndex].teacherId
            );
        if (teacher == teacherIndexesById.end())
        {
            continue;
        }
        groups[teacher->second].classIndexes.push_back(classIndex);
    }

    std::sort(
        groups.begin(),
        groups.end(),
        [&teachers, &compareTeacherNames](
            const SubPrepPrintGroupOrder& leftGroup,
            const SubPrepPrintGroupOrder& rightGroup
        )
        {
            const SubPrepPrintTeacherOrderFact& left =
                teachers[leftGroup.teacherIndex];
            const SubPrepPrintTeacherOrderFact& right =
                teachers[rightGroup.teacherIndex];

            if (left.hasEnglishName != right.hasEnglishName)
            {
                return left.hasEnglishName;
            }

            const int englishComparison = compareTeacherNames(
                leftGroup.teacherIndex,
                rightGroup.teacherIndex,
                SubPrepPrintTeacherNameField::English
                );
            if (englishComparison != 0)
            {
                return englishComparison < 0;
            }

            const int koreanComparison = compareTeacherNames(
                leftGroup.teacherIndex,
                rightGroup.teacherIndex,
                SubPrepPrintTeacherNameField::Korean
                );
            if (koreanComparison != 0)
            {
                return koreanComparison < 0;
            }

            return left.teacherId < right.teacherId;
        }
        );

    for (SubPrepPrintGroupOrder& group : groups)
    {
        std::sort(
            group.classIndexes.begin(),
            group.classIndexes.end(),
            [&classes](const std::size_t leftIndex, const std::size_t rightIndex)
            {
                const SubPrepPrintClassOrderFact& left = classes[leftIndex];
                const SubPrepPrintClassOrderFact& right = classes[rightIndex];
                if (left.gradeOrder != right.gradeOrder)
                {
                    return left.gradeOrder < right.gradeOrder;
                }
                if (left.levelOrder != right.levelOrder)
                {
                    return left.levelOrder < right.levelOrder;
                }
                if (left.weekdayOrder != right.weekdayOrder)
                {
                    return left.weekdayOrder < right.weekdayOrder;
                }
                if (left.startMinute != right.startMinute)
                {
                    return left.startMinute < right.startMinute;
                }
                return left.classId < right.classId;
            }
            );
    }

    return groups;
}

} // namespace ClassMngr::Next::Application
