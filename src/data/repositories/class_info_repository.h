#pragma once

#include "core/enums/schedule_type.h"
#include "core/result.h"
#include "domain/models/class_conflict.h"
#include "domain/models/class_info.h"
#include "domain/models/class_teacher_assignment.h"

#include <QList>
#include <QSqlDatabase>

class ClassInfoRepository
{
public:
    explicit ClassInfoRepository(
        QSqlDatabase& database
        );

    [[nodiscard]] Status saveClassInfo(
        const ClassInfo& info
        );

    [[nodiscard]] Status saveClassNotes(
        int classId,
        const QString& notes,
        const QString& timeFillerActivities
        );

    [[nodiscard]] Result<ClassInfo> loadClassInfo(
        int classId
        );

    [[nodiscard]] Result<QList<ClassTeacherAssignment>>
        loadClassTeacherAssignments();

    [[nodiscard]] Result<QList<ClassInfo>> loadScheduleClassInfos();

    [[nodiscard]] Result<QList<ClassConflict>> getClassTimeConflicts(
        int classId,
        const QList<ClassTime>& times,
        ScheduleType type
        );

private:
    QSqlDatabase& m_database;
};
