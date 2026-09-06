#pragma once

#include "core/result.h"
#include "domain/models/class_transfer.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>

#include <memory>

namespace classmngr::engine
{
class SqliteDatabase;
}

class ClassTransferRepository
{
public:
    explicit ClassTransferRepository(const QString& databasePath);
    // Compatibility-only constructor for retained Qt SQL tests/adapters.
    explicit ClassTransferRepository(
        QSqlDatabase& database
        );
    ~ClassTransferRepository();

    [[nodiscard]] Result<ClassTransferPackage> buildPackage(
        const QList<int>& classIds
        );

    [[nodiscard]] Result<ClassImportPreview> previewImport(
        const ClassTransferPackage& package
        );

    [[nodiscard]] Result<ClassImportSummary> importClasses(
        const ClassTransferPackage& package,
        const ClassImportPlan& plan
        );

private:
    [[nodiscard]] Status ensureEngineDatabase(
        const QString& operation
        );

    QString m_databasePath;
    bool m_compatibilityDatabaseWasOpen = true;
    std::unique_ptr<classmngr::engine::SqliteDatabase> m_engineDatabase;
    QString m_engineDatabasePath;
};
