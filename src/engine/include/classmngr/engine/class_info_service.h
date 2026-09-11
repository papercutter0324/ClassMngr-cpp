#pragma once

#include "classmngr/engine/class_info.h"
#include "classmngr/engine/result.h"

#include <string_view>

namespace classmngr::engine
{

class SqliteDatabase;

// Validated class-information persistence is shared by native and retained
// desktop adapters.  The service owns the class_info/time-table workflow;
// presentation layers only translate their models at this boundary.
class ClassInfoService final
{
public:
    explicit ClassInfoService(
        SqliteDatabase& database
        );

    [[nodiscard]] Status save(
        const ClassInfo& info
        );

    [[nodiscard]] Status saveNotes(
        int classId,
        std::string_view notes,
        std::string_view timeFillerActivities
        );

    [[nodiscard]] Result<ClassInfo> load(
        int classId
        );

private:
    SqliteDatabase& m_database;
};

} // namespace classmngr::engine
