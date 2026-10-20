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
    std::optional<Domain::ScheduleTime> scheduleTime;
    std::string dayLabel;
    std::string startLabel;
    std::string endLabel;
};

[[nodiscard]] inline ScheduleImportStateTime makeScheduleImportStateTime(
    std::optional<Domain::Weekday> weekday,
    int startMinute,
    int endMinute,
    std::string dayLabel,
    std::string startLabel,
    std::string endLabel
    )
{
    std::optional<Domain::ScheduleTime> scheduleTime;
    if (weekday)
    {
        scheduleTime = Domain::ScheduleTime::fromMinutes(
            *weekday,
            startMinute,
            endMinute
            );
    }

    return {
        std::move(scheduleTime),
        std::move(dayLabel),
        std::move(startLabel),
        std::move(endLabel)
    };
}

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
    if (!stateTime.scheduleTime)
    {
        return std::nullopt;
    }

    return ScheduleImportProjectedTime{
        *stateTime.scheduleTime,
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
