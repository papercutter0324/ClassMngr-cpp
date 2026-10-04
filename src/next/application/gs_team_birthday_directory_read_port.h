#pragma once

#include "next/application/gs_team_birthday_directory_read_snapshot.h"
#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

using GsTeamBirthdayDirectoryReadResult =
    Domain::Result<GsTeamBirthdayDirectorySnapshot>;

class GsTeamBirthdayDirectoryReadPort
{
public:
    virtual ~GsTeamBirthdayDirectoryReadPort() = default;

    [[nodiscard]] virtual GsTeamBirthdayDirectoryReadResult
    readGsTeamBirthdayDirectory() const = 0;
};

} // namespace ClassMngr::Next::Application
