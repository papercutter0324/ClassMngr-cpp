#pragma once

#include "classmngr/engine/result.h"
#include "classmngr/engine/roster.h"

#include <utility>
#include <vector>

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

    [[nodiscard]] Status saveBatch(
        const std::vector<std::pair<int, Roster>>& rosters
        );

private:
    [[nodiscard]] Status saveContents(
        int classId,
        const Roster& roster
        );

    SqliteDatabase& m_database;
};

} // namespace classmngr::engine
