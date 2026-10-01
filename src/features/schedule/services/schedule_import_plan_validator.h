#pragma once

#include "core/result.h"
#include "domain/models/schedule_import.h"
#include "next/application/schedule_import_apply_use_case.h"

#include <QHash>
#include <QString>

struct ValidatedScheduleImportPlan
{
    QHash<QString, ScheduleImportTeacherResolution> teacherResolutions;
    QHash<int, ScheduleImportClassResolution> classResolutions;
};

class ScheduleImportPlanValidator final
{
public:
    [[nodiscard]] static Result<ValidatedScheduleImportPlan> validate(
        const ScheduleImportPlan& plan
        );
    [[nodiscard]] static QString policyFailureMessage(
        const ClassMngr::Next::Application::ScheduleImportApplyRequest& request,
        const ClassMngr::Next::Application::ScheduleImportPlanEligibilityIssue& issue
        );
};
