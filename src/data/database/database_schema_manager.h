#pragma once

#include "core/result.h"

#include <QString>

class QSqlDatabase;

class DatabaseSchemaManager
{
public:
    static constexpr int LatestSchemaVersion = 6;

    [[nodiscard]] static Status ensureSchema(QSqlDatabase& database);
    [[nodiscard]] static Status ensureSchema(
        QSqlDatabase& database,
        const QString& databaseFilePath
        );
    [[nodiscard]] static Status enableForeignKeyEnforcement(
        QSqlDatabase& database
        );
    [[nodiscard]] static Result<int> schemaVersion(QSqlDatabase& database);
};
