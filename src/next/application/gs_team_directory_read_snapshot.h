#pragma once

#include "next/domain/gs_team_member_id.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct GsTeamDirectoryEntry final
{
    Domain::GsTeamMemberId id;
    std::u16string name;
    std::u16string koreanName;
    std::u16string position;
    std::u16string phoneNumber;
    std::u16string birthday;

    friend bool operator==(
        const GsTeamDirectoryEntry&,
        const GsTeamDirectoryEntry&
        ) = default;
};

using GsTeamDirectorySnapshot = std::vector<GsTeamDirectoryEntry>;

} // namespace ClassMngr::Next::Application
