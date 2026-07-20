#pragma once

#include "core/result.h"
#include "domain/models/teacher_import.h"

#include <QSqlDatabase>

class TeacherImportRepository
{
public:
    static constexpr auto LatestSourceDateSetting =
        "teacher_import/latest_source_date";

    explicit TeacherImportRepository(QSqlDatabase& database);

    [[nodiscard]] Result<TeacherImportSummary> importTeachers(
        const TeacherImportPlan& plan
        );

private:
    QSqlDatabase& m_database;
};
