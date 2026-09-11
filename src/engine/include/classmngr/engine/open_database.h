#pragma once

#include "classmngr/engine/sqlite_database.h"

#include <memory>
#include <string_view>

namespace classmngr::engine
{

struct OpenDatabaseOptions
{
    SqliteOpenOptions sqlite;
    bool createParentDirectories = true;
};

class OpenDatabase final
{
public:
    OpenDatabase() = delete;

    [[nodiscard]] static Result<std::unique_ptr<SqliteDatabase>> execute(
        std::string_view databasePath,
        const OpenDatabaseOptions& options = {}
        );
};

} // namespace classmngr::engine
