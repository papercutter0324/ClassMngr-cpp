#pragma once

#include "next/application/gs_team_directory_read_snapshot.h"
#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

using GsTeamDirectoryReadResult =
    Domain::Result<GsTeamDirectorySnapshot>;

class GsTeamDirectoryReadPort
{
public:
    virtual ~GsTeamDirectoryReadPort() = default;

    [[nodiscard]] virtual GsTeamDirectoryReadResult
    readGsTeamDirectory() const = 0;
};

} // namespace ClassMngr::Next::Application
