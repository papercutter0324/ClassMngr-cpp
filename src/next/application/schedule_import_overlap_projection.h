#pragma once

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct ScheduleImportStateTime
{
    int dayIndex = -1;
    int startMinute = -1;
    int endMinute = -1;
    std::string dayLabel;
    std::string startLabel;
    std::string endLabel;
};

struct ScheduleImportOverlapSchedule
{
    std::string classLabel;
    std::vector<ScheduleImportStateTime> times;
};

struct ScheduleImportScheduleConflict
{
    std::string classLabel;
    std::string conflictingClassLabel;
    ScheduleImportStateTime time;
    ScheduleImportStateTime conflictingTime;
};

[[nodiscard]] inline bool scheduleImportTimesOverlap(
    const ScheduleImportStateTime& left,
    const ScheduleImportStateTime& right
    )
{
    const auto isValid = [](const ScheduleImportStateTime& time)
    {
        return time.dayIndex >= 0
            && time.dayIndex <= 6
            && time.startMinute >= 0
            && time.startMinute < 24 * 60
            && time.endMinute > time.startMinute
            && time.endMinute < 24 * 60;
    };

    return isValid(left)
        && isValid(right)
        && left.dayIndex == right.dayIndex
        && left.startMinute < right.endMinute
        && right.startMinute < left.endMinute;
}

// Returns conflicts in schedule order, then meeting order. Each conflict names
// the later schedule first, matching the existing review and apply messages.
[[nodiscard]] inline std::vector<ScheduleImportScheduleConflict>
projectScheduleImportOverlaps(
    const std::vector<ScheduleImportOverlapSchedule>& schedules
    )
{
    struct ProjectedOccurrence
    {
        std::string classLabel;
        ScheduleImportStateTime time;
    };

    std::vector<ProjectedOccurrence> projectedOccurrences;
    std::vector<ScheduleImportScheduleConflict> conflicts;
    for (const ScheduleImportOverlapSchedule& schedule : schedules)
    {
        for (const ScheduleImportStateTime& time : schedule.times)
        {
            for (const ProjectedOccurrence& existing : projectedOccurrences)
            {
                if (scheduleImportTimesOverlap(time, existing.time))
                {
                    conflicts.push_back(
                        {
                            schedule.classLabel,
                            existing.classLabel,
                            time,
                            existing.time
                        }
                        );
                }
            }
            projectedOccurrences.push_back(
                {schedule.classLabel, time}
                );
        }
    }
    return conflicts;
}

} // namespace ClassMngr::Next::Application
