#pragma once

#include "core/result.h"
#include "classmngr/engine/teacher_import.h"
#include "domain/models/teacher_import.h"

#include <QDate>
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

    explicit TeacherImportRepository(const QString& databasePath);
    // Compatibility-only constructor for retained Qt SQL tests/adapters.
    explicit TeacherImportRepository(QSqlDatabase& database);
    ~TeacherImportRepository();

    [[nodiscard]] Result<TeacherImportSummary> importTeachers(
        const TeacherImportPlan& plan
        );

    [[nodiscard]] Result<classmngr::engine::TeacherImportDateDecision>
        compareLatestSourceDate(const QDate& sourceDate);

private:
    [[nodiscard]] Status ensureEngineDatabase(
        const QString& operation
        );

    QString m_databasePath;
    bool m_compatibilityDatabaseWasOpen = true;
    std::unique_ptr<classmngr::engine::SqliteDatabase> m_engineDatabase;
    QString m_engineDatabasePath;
};
