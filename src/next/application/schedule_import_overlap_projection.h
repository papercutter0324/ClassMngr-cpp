#pragma once

#include "next/domain/schedule_time.h"

#include <optional>
#include <string>
#include <utility>
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

struct ScheduleImportProjectedTime
{
    Domain::ScheduleTime scheduleTime;
    std::string dayLabel;
    std::string startLabel;
    std::string endLabel;
};

[[nodiscard]] inline std::optional<ScheduleImportProjectedTime>
projectScheduleImportStateTime(const ScheduleImportStateTime& stateTime)
{
    auto scheduleTime = Domain::ScheduleTime::fromMinutes(
        stateTime.dayIndex,
        stateTime.startMinute,
        stateTime.endMinute
        );
    if (!scheduleTime)
    {
        return std::nullopt;
    }

    return ScheduleImportProjectedTime{
        std::move(*scheduleTime),
        stateTime.dayLabel,
        stateTime.startLabel,
        stateTime.endLabel
    };
}

struct ScheduleImportOverlapSchedule
{
    std::string classLabel;
    std::vector<ScheduleImportProjectedTime> times;
};

struct ScheduleImportScheduleConflict
{
    std::string classLabel;
    std::string conflictingClassLabel;
    ScheduleImportProjectedTime time;
    ScheduleImportProjectedTime conflictingTime;
};

[[nodiscard]] inline bool scheduleImportTimesOverlap(
    const ScheduleImportProjectedTime& left,
    const ScheduleImportProjectedTime& right
    )
{
    return left.scheduleTime.overlaps(right.scheduleTime);
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
        ScheduleImportProjectedTime time;
    };

    std::vector<ProjectedOccurrence> projectedOccurrences;
    std::vector<ScheduleImportScheduleConflict> conflicts;
    for (const ScheduleImportOverlapSchedule& schedule : schedules)
    {
        for (const ScheduleImportProjectedTime& time : schedule.times)
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
