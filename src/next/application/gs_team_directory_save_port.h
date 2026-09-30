#pragma once

#include "next/application/gs_team_directory_save.h"

namespace ClassMngr::Next::Application
{

class GsTeamDirectorySavePort
{
public:
    virtual ~GsTeamDirectorySavePort() = default;

    [[nodiscard]] virtual Domain::Result<void> saveGsTeamDirectory(
        const GsTeamDirectorySaveRequest& request
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
