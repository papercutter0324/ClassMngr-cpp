#pragma once

#include "core/result.h"

#include <QString>

class QSqlDatabase;

class DatabaseSchemaManager
{
public:
    static constexpr int LatestSchemaVersion = 6;
    static constexpr int MinimumSupportedSchemaVersion = 6;

    enum class Initialization
    {
        AllowEmptyDatabase,
        ExistingDatabase
    };

    // Creation must be requested explicitly; ordinary validation never
    // initializes an existing unversioned database, even when it is empty.
    [[nodiscard]] static Status createSchema(QSqlDatabase& database);
    [[nodiscard]] static Status ensureSchema(QSqlDatabase& database);
    [[nodiscard]] static Status ensureSchema(
        QSqlDatabase& database,
        const QString& databaseFilePath,
        Initialization initialization = Initialization::ExistingDatabase
        );
    [[nodiscard]] static Status enableForeignKeyEnforcement(
        QSqlDatabase& database
        );
    [[nodiscard]] static Result<int> schemaVersion(QSqlDatabase& database);
};
