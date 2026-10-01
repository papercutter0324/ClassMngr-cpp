#pragma once

#include "next/application/schedule_import_review_decisions.h"
#include "next/application/schedule_import_state_snapshot.h"

#include <charconv>
#include <set>
#include <system_error>

namespace ClassMngr::Next::Application
{

namespace schedule_import_schedules_cleared_projection_detail
{
// Match the review dialog's existing full-parse integer conversion so typed
// IDs retain its persisted-ID comparison behavior.
[[nodiscard]] inline int scheduleImportReviewLegacyClassId(
    const Domain::ClassId& id
    )
{
    int value = -1;
    const std::string& text = id.value();
    const auto [end, error] = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value
        );
    return error == std::errc{} && end == text.data() + text.size()
        ? value
        : -1;
}
} // namespace schedule_import_schedules_cleared_projection_detail

[[nodiscard]] inline int projectScheduleImportSchedulesCleared(
    const ScheduleImportStateSnapshot& snapshot,
    const ScheduleImportReviewDecisionRequest& decisions,
    const ScheduleImportStateKind kind,
    const ScheduleImportStateIntensiveMode intensiveMode
    )
{
    if (kind == ScheduleImportStateKind::Intensive
        && intensiveMode == ScheduleImportStateIntensiveMode::UpdateExisting)
    {
        return 0;
    }

    std::set<int> selectedPositiveTargets;
    for (const ScheduleImportReviewClassResolution& resolution :
         decisions.classes)
    {
        if (!resolution.targetClassId)
        {
            continue;
        }

        const int targetId =
            schedule_import_schedules_cleared_projection_detail::
                scheduleImportReviewLegacyClassId(
                    *resolution.targetClassId
                    );
        if (targetId > 0)
        {
            selectedPositiveTargets.insert(targetId);
        }
    }

    int cleared = 0;
    for (const ScheduleImportStateReadClassSnapshot& classroom :
         snapshot.classes)
    {
        const auto& selectedTimes = kind == ScheduleImportStateKind::Intensive
            ? classroom.intensiveTimes
            : classroom.normalTimes;
        if (selectedTimes.empty())
        {
            continue;
        }

        const int classId =
            schedule_import_schedules_cleared_projection_detail::
                scheduleImportReviewLegacyClassId(classroom.id);
        if (!selectedPositiveTargets.contains(classId))
        {
            ++cleared;
        }
    }

    return cleared;
}

} // namespace ClassMngr::Next::Application
