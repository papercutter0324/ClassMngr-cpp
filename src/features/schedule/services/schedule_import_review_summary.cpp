#include "schedule_import_review_summary.h"

ScheduleImportReviewSummary ScheduleImportReviewSummaryBuilder::build(
    const ScheduleImportPlan& plan,
    int schedulesCleared
    )
{
    ScheduleImportReviewSummary summary;
    summary.schedulesCleared = schedulesCleared;
    summary.ignoredCells = plan.diagnostics.size();
    for (const ScheduleImportTeacherResolution& resolution : plan.teachers)
    {
        if (resolution.action == ScheduleImportTeacherAction::Create)
        {
            ++summary.teachersCreated;
        }
        else if (resolution.action == ScheduleImportTeacherAction::UpdateRoom)
        {
            ++summary.teacherRoomsUpdated;
        }
        else if (resolution.action == ScheduleImportTeacherAction::Skip)
        {
            ++summary.teachersSkipped;
        }
    }
    for (const ScheduleImportClassResolution& resolution : plan.classes)
    {
        if (resolution.action == ScheduleImportClassAction::CreateNew)
        {
            ++summary.classesCreated;
        }
        else if (resolution.action == ScheduleImportClassAction::UpdateExisting)
        {
            ++summary.classesUpdated;
        }
        else if (resolution.action == ScheduleImportClassAction::Skip)
        {
            ++summary.classesSkipped;
        }
    }
    return summary;
}
