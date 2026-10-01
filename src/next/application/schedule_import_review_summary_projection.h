#pragma once

#include "next/application/schedule_import_review_decisions.h"

namespace ClassMngr::Next::Application
{

struct ScheduleImportReviewSummary final
{
    int teachersCreated = 0;
    int teacherRoomsUpdated = 0;
    int teachersSkipped = 0;
    int classesCreated = 0;
    int classesUpdated = 0;
    int classesSkipped = 0;
    int schedulesCleared = 0;
    int ignoredCells = 0;
};

[[nodiscard]] inline ScheduleImportReviewSummary
projectScheduleImportReviewSummary(
    const ScheduleImportReviewDecisionRequest& decisions,
    const int ignoredDiagnosticCount,
    const int schedulesCleared
    )
{
    ScheduleImportReviewSummary summary;
    summary.schedulesCleared = schedulesCleared;
    summary.ignoredCells = ignoredDiagnosticCount;

    for (const ScheduleImportReviewTeacherResolution& teacher : decisions.teachers)
    {
        switch (teacher.action)
        {
        case ScheduleImportReviewTeacherAction::Create:
            ++summary.teachersCreated;
            break;
        case ScheduleImportReviewTeacherAction::UpdateRoom:
            ++summary.teacherRoomsUpdated;
            break;
        case ScheduleImportReviewTeacherAction::Skip:
            ++summary.teachersSkipped;
            break;
        case ScheduleImportReviewTeacherAction::Unselected:
        case ScheduleImportReviewTeacherAction::Reuse:
        case ScheduleImportReviewTeacherAction::Invalid:
            break;
        }
    }

    for (const ScheduleImportReviewClassResolution& classroom : decisions.classes)
    {
        switch (classroom.action)
        {
        case ScheduleImportReviewClassAction::CreateNew:
            ++summary.classesCreated;
            break;
        case ScheduleImportReviewClassAction::UpdateExisting:
            ++summary.classesUpdated;
            break;
        case ScheduleImportReviewClassAction::Skip:
            ++summary.classesSkipped;
            break;
        case ScheduleImportReviewClassAction::Unselected:
        case ScheduleImportReviewClassAction::Invalid:
            break;
        }
    }

    return summary;
}

} // namespace ClassMngr::Next::Application
