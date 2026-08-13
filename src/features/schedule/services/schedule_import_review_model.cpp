#include "schedule_import_review_model.h"

ScheduleImportPlan ScheduleImportReviewModel::buildPlan(
    const ScheduleImportReviewContext& context,
    const QList<ScheduleImportTeacherResolution>& teachers,
    const QList<ScheduleImportClassResolution>& classes
    )
{
    ScheduleImportPlan plan;
    plan.kind = context.kind;
    plan.intensiveMode = context.intensiveMode;
    plan.selectedUserName = context.selectedUserName;
    plan.saveProfileNameIfBlank = context.saveProfileNameIfBlank;
    plan.updateProfileName = context.updateProfileName;
    plan.unknownCellsAcknowledged = context.unknownCellsAcknowledged;
    plan.candidates = context.candidates;
    plan.intensiveSlotStates = context.intensiveSlotStates;
    plan.diagnostics = context.diagnostics;
    plan.teachers = teachers;
    plan.classes = classes;
    return plan;
}
