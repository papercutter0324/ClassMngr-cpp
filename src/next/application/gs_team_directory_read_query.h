#pragma once

#include "next/application/gs_team_directory_read_port.h"

namespace ClassMngr::Next::Application
{

class GsTeamDirectoryReadQuery final
{
public:
    explicit GsTeamDirectoryReadQuery(
        const GsTeamDirectoryReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] GsTeamDirectoryReadResult execute() const
    {
        return m_port.readGsTeamDirectory();
    }

private:
    const GsTeamDirectoryReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
