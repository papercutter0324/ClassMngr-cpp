#pragma once

#include "core/result.h"
#include "domain/models/schedule_import.h"

#include <QHash>

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
};
