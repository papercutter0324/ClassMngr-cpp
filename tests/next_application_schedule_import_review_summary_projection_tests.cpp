#include "next/application/schedule_import_review_summary_projection.h"

#include <stdexcept>

using namespace ClassMngr::Next::Application;

namespace
{
void require(const bool condition)
{
    if (!condition)
        throw std::runtime_error("Schedule import review summary assertion failed");
}
}

int main()
{
    ScheduleImportReviewDecisionRequest decisions;
    decisions.teachers = {
        {"create-1", ScheduleImportReviewTeacherAction::Create},
        {"create-2", ScheduleImportReviewTeacherAction::Create},
        {"update-1", ScheduleImportReviewTeacherAction::UpdateRoom},
        {"skip-1", ScheduleImportReviewTeacherAction::Skip},
        {"skip-2", ScheduleImportReviewTeacherAction::Skip},
        {"reuse", ScheduleImportReviewTeacherAction::Reuse},
        {"unselected", ScheduleImportReviewTeacherAction::Unselected},
        {"invalid", ScheduleImportReviewTeacherAction::Invalid}
    };
    decisions.classes = {
        {0, ScheduleImportReviewClassAction::CreateNew},
        {1, ScheduleImportReviewClassAction::CreateNew},
        {2, ScheduleImportReviewClassAction::UpdateExisting},
        {3, ScheduleImportReviewClassAction::Skip},
        {4, ScheduleImportReviewClassAction::Skip},
        {5, ScheduleImportReviewClassAction::Unselected},
        {6, ScheduleImportReviewClassAction::Invalid}
    };

    const ScheduleImportReviewSummary summary =
        projectScheduleImportReviewSummary(decisions, 3, 9);
    require(summary.teachersCreated == 2);
    require(summary.teacherRoomsUpdated == 1);
    require(summary.teachersSkipped == 2);
    require(summary.classesCreated == 2);
    require(summary.classesUpdated == 1);
    require(summary.classesSkipped == 2);
    require(summary.schedulesCleared == 9);
    require(summary.ignoredCells == 3);
}
