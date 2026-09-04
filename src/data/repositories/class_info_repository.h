#pragma once

#include "core/enums/schedule_type.h"
#include "core/result.h"
#include "domain/models/class_conflict.h"
#include "domain/models/class_info.h"
#include "domain/models/class_teacher_assignment.h"

#include <QList>
#include <QSqlDatabase>

#include <memory>

namespace classmngr::engine
{
class SqliteDatabase;
}

class ClassInfoRepository
{
public:
    explicit ClassInfoRepository(const QString& databasePath);
    // Compatibility-only constructor for retained Qt SQL tests/adapters.
    explicit ClassInfoRepository(
        QSqlDatabase& database
        );
    ~ClassInfoRepository();

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
    [[nodiscard]] Status ensureEngineDatabase(
        const QString& operation,
        int classId = -1
        );

    QString m_databasePath;
    bool m_compatibilityDatabaseWasOpen = true;
    std::unique_ptr<classmngr::engine::SqliteDatabase> m_engineDatabase;
    QString m_engineDatabasePath;
};
