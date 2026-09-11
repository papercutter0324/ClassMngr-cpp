#pragma once

#include "classmngr/engine/gs_team_member.h"
#include "classmngr/engine/result.h"

#include <vector>

namespace classmngr::engine
{

class SqliteDatabase;

class GsTeamService final
{
public:
    explicit GsTeamService(
        SqliteDatabase& database
        );

    [[nodiscard]] Result<std::vector<GsTeamMember>> list() const;

    [[nodiscard]] Status saveDirectory(
        const std::vector<GsTeamMember>& members,
        const std::vector<int>& deletedIds
        );

private:
    SqliteDatabase& m_database;
};

} // namespace classmngr::engine
