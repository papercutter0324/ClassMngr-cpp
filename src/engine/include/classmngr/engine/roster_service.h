#pragma once

#include "classmngr/engine/result.h"
#include "classmngr/engine/roster.h"

namespace classmngr::engine
{

class SqliteDatabase;

class RosterService final
{
public:
    explicit RosterService(
        SqliteDatabase& database
        );

    [[nodiscard]] Result<Roster> load(
        int classId
        );

    [[nodiscard]] Status save(
        int classId,
        const Roster& roster
        );

private:
    SqliteDatabase& m_database;
};

} // namespace classmngr::engine
