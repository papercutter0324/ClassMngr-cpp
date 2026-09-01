#pragma once

#include "core/result.h"
#include "domain/models/schedule_import.h"

#include <QSqlDatabase>

#include <memory>

namespace classmngr::engine
{
class SqliteDatabase;
}

class ScheduleImportRepository
{
public:
    explicit ScheduleImportRepository(
        QSqlDatabase& database
        );
    ~ScheduleImportRepository();

    [[nodiscard]] Result<ScheduleImportPreview> preview(
        const ScheduleImportUserBlock& user,
        ScheduleImportKind kind
        );

    [[nodiscard]] Result<ScheduleImportSummary> apply(
        const ScheduleImportPlan& plan
        );

private:
    [[nodiscard]] Status ensureEngineDatabase(
        const QString& operation
        );

    QSqlDatabase& m_database;
    std::unique_ptr<classmngr::engine::SqliteDatabase> m_engineDatabase;
    QString m_engineDatabasePath;
};
