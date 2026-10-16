#pragma once

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

namespace ClassMngr::Next::Application
{

enum class SubPrepSelectedClassScheduleEntryKind
{
    RegularClass,
    TestingClass
};

struct SubPrepSelectedClassScheduleEntry final
{
    int classId{-1};
    SubPrepSelectedClassScheduleEntryKind kind =
        SubPrepSelectedClassScheduleEntryKind::RegularClass;
};

struct SubPrepSelectedClassScheduleCell final
{
    std::u16string day;
    std::vector<SubPrepSelectedClassScheduleEntry> entries;
};

struct SubPrepSelectedClassScheduleRow final
{
    std::vector<SubPrepSelectedClassScheduleCell> cells;
};

// Projects selected class identifiers in schedule display order. Day values
// are compared as exact UTF-16 code-unit sequences; every entry kind is
// included, and the first positive occurrence of each ID wins.
[[nodiscard]] inline std::vector<int> projectSubPrepSelectedClassIds(
    const std::vector<std::u16string>& selectedDays,
    const std::vector<SubPrepSelectedClassScheduleRow>& scheduleRows
    )
{
    if (selectedDays.empty() || scheduleRows.empty())
    {
        return {};
    }

    std::vector<int> classIds;
    std::unordered_set<int> seenClassIds;

    for (const SubPrepSelectedClassScheduleRow& row : scheduleRows)
    {
        for (const SubPrepSelectedClassScheduleCell& cell : row.cells)
        {
            if (std::find(selectedDays.begin(), selectedDays.end(), cell.day)
                == selectedDays.end())
            {
                continue;
            }

            for (const SubPrepSelectedClassScheduleEntry& entry : cell.entries)
            {
                if (entry.classId > 0
                    && seenClassIds.insert(entry.classId).second)
                {
                    classIds.push_back(entry.classId);
                }
            }
        }
    }

    return classIds;
}

} // namespace ClassMngr::Next::Application
