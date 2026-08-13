#pragma once

#include "core/result.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/schedule_import.h"
#include "domain/models/teacher.h"
#include "features/schedule/services/schedule_import_plan_validator.h"

#include <QHash>
#include <QList>

class ScheduleImportStateValidator final
{
public:
    [[nodiscard]] static Status validate(
        const ScheduleImportPlan& plan,
        const ValidatedScheduleImportPlan& validatedPlan,
        const QList<Teacher>& existingTeachers,
        const QList<Classroom>& existingClasses,
        const QHash<int, ClassInfo>& existingInfo
        );
};
