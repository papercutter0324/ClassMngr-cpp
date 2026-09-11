#pragma once

#include "classmngr/engine/class_info.h"
#include "classmngr/engine/class_schedule.h"
#include "classmngr/engine/result.h"

#include <vector>

namespace classmngr::engine
{

class SqliteDatabase;

// Renderer-neutral class schedule reads shared by the native and retained
// presentation stacks.  Testing classes are omitted from snapshots and
// assignments; conflict checks preserve the existing schedule-table rules.
class ClassScheduleService final
{
public:
    explicit ClassScheduleService(
        SqliteDatabase& database
        );

    [[nodiscard]] Result<std::vector<ClassTeacherAssignment>>
        loadClassTeacherAssignments();

    [[nodiscard]] Result<std::vector<ClassInfo>>
        loadScheduleClassInfos();

    [[nodiscard]] Result<std::vector<ClassConflict>> getClassTimeConflicts(
        int classId,
        const std::vector<ClassTime>& times,
        ScheduleType type
        );

    [[nodiscard]] std::vector<ClassConflict> findConflicts(
        const std::vector<ClassScheduleEntry>& candidates,
        const std::vector<ClassScheduleEntry>& existing
        ) const;

private:
    SqliteDatabase& m_database;
};

} // namespace classmngr::engine
