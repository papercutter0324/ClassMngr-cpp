#pragma once

#include "data/repositories/repository_error.h"

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
    [[nodiscard]] static RepositoryStatus createSchema(QSqlDatabase& database);
    [[nodiscard]] static RepositoryStatus ensureSchema(QSqlDatabase& database);
    [[nodiscard]] static RepositoryStatus ensureSchema(
        QSqlDatabase& database,
        const QString& databaseFilePath,
        Initialization initialization = Initialization::ExistingDatabase
        );
    [[nodiscard]] static RepositoryStatus enableForeignKeyEnforcement(
        QSqlDatabase& database
        );
    [[nodiscard]] static RepositoryResult<int> schemaVersion(QSqlDatabase& database);
};
