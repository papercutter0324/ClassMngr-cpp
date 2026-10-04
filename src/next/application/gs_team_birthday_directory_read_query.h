#pragma once

#include "next/application/gs_team_birthday_directory_read_port.h"

namespace ClassMngr::Next::Application
{

class GsTeamBirthdayDirectoryReadQuery final
{
public:
    explicit GsTeamBirthdayDirectoryReadQuery(
        const GsTeamBirthdayDirectoryReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] GsTeamBirthdayDirectoryReadResult execute() const
    {
        return m_port.readGsTeamBirthdayDirectory();
    }

private:
    const GsTeamBirthdayDirectoryReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
