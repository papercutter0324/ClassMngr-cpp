#pragma once

#include "next/domain/domain_types.h"

#include <algorithm>
#include <charconv>
#include <string>
#include <system_error>
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
    Domain::ClassId classId;
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
[[nodiscard]] inline std::vector<Domain::ClassId>
projectSubPrepSelectedClassIds(
    const std::vector<std::u16string>& selectedDays,
    const std::vector<SubPrepSelectedClassScheduleRow>& scheduleRows
    )
{
    if (selectedDays.empty() || scheduleRows.empty())
    {
        return {};
    }

    std::vector<Domain::ClassId> classIds;
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
                const std::string& value = entry.classId.value();
                int parsedId = 0;
                const auto [end, error] = std::from_chars(
                    value.data(),
                    value.data() + value.size(),
                    parsedId,
                    10
                    );
                if (error != std::errc{}
                    || end != value.data() + value.size()
                    || parsedId <= 0
                    || !seenClassIds.insert(parsedId).second)
                {
                    continue;
                }

                const auto canonicalId = Domain::ClassId::fromString(
                    std::to_string(parsedId)
                    );
                if (canonicalId)
                {
                    classIds.push_back(*canonicalId);
                }
            }
        }
    }

    return classIds;
}

} // namespace ClassMngr::Next::Application
