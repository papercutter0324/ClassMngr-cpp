#pragma once

#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/schedule_import.h"
#include "domain/models/teacher.h"

#include <QHash>
#include <QList>

class ScheduleImportMatcher final
{
public:
    [[nodiscard]] static ScheduleImportPreview preview(
        const ScheduleImportUserBlock& user,
        ScheduleImportKind kind,
        const QList<Teacher>& teachers,
        const QList<Classroom>& classrooms,
        const QHash<int, ClassInfo>& classInfo
        );
};
