#pragma once

#include "core/enums/schedule_type.h"
#include "domain/models/class_conflict.h"
#include "domain/models/class_info.h"

#include <QList>
#include <QSqlDatabase>

class ClassInfoRepository
{
public:
    explicit ClassInfoRepository(
        QSqlDatabase& database
        );

    bool saveClassInfo(
        const ClassInfo& info
        );

    bool saveClassNotes(
        int classId,
        const QString& notes,
        const QString& timeFillerActivities
        );

    ClassInfo loadClassInfo(
        int classId
        );

    QList<ClassConflict> getClassTimeConflicts(
        int classId,
        const QList<ClassTime>& times,
        ScheduleType type
        );

private:
    QSqlDatabase& m_database;
};
