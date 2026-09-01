#pragma once

#include "core/result.h"
#include "domain/models/teacher_import.h"

#include <QSqlDatabase>

#include <memory>

namespace classmngr::engine
{
class SqliteDatabase;
}

class TeacherImportRepository
{
public:
    static constexpr auto LatestSourceDateSetting =
        "teacher_import/latest_source_date";

    explicit TeacherImportRepository(QSqlDatabase& database);
    ~TeacherImportRepository();

    [[nodiscard]] Result<TeacherImportSummary> importTeachers(
        const TeacherImportPlan& plan
        );

private:
    [[nodiscard]] Status ensureEngineDatabase(
        const QString& operation
        );

    QSqlDatabase& m_database;
    std::unique_ptr<classmngr::engine::SqliteDatabase> m_engineDatabase;
    QString m_engineDatabasePath;
};
