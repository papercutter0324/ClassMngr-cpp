#pragma once

#include "classmngr/engine/result.h"
#include "classmngr/engine/teacher_import.h"

#include <string_view>

namespace classmngr::engine
{

class SqliteDatabase;

class TeacherImportService final
{
public:
    static constexpr std::string_view LatestSourceDateSetting =
        "teacher_import/latest_source_date";

    explicit TeacherImportService(
        SqliteDatabase& database
        );

    [[nodiscard]] Result<TeacherImportSummary> importTeachers(
        const TeacherImportPlan& plan
        );

private:
    SqliteDatabase& m_database;
};

} // namespace classmngr::engine
