#pragma once

#include "core/result.h"

class QSqlDatabase;

class DatabaseSchemaManager
{
public:
    static constexpr int LatestSchemaVersion = 6;

    [[nodiscard]] static Status ensureSchema(QSqlDatabase& database);
    [[nodiscard]] static Status enableForeignKeyEnforcement(
        QSqlDatabase& database
        );
    [[nodiscard]] static Result<int> schemaVersion(QSqlDatabase& database);
};
