#pragma once

#include "classmngr/engine/result.h"

namespace classmngr::engine
{

class SqliteDatabase;

class DatabaseSchemaManager final
{
public:
    static constexpr int LatestSchemaVersion = 6;

    [[nodiscard]] static Status ensureSchema(
        SqliteDatabase& database
        );

    [[nodiscard]] static Status enableForeignKeyEnforcement(
        SqliteDatabase& database
        );

    [[nodiscard]] static Result<int> schemaVersion(
        const SqliteDatabase& database
        );
};

} // namespace classmngr::engine
