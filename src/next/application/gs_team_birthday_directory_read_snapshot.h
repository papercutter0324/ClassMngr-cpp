#pragma once

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct GsTeamBirthdayDirectoryEntry final
{
    std::u16string name;
    std::u16string koreanName;
    std::u16string position;
    std::u16string birthday;

    friend bool operator==(
        const GsTeamBirthdayDirectoryEntry&,
        const GsTeamBirthdayDirectoryEntry&
        ) = default;
};

using GsTeamBirthdayDirectorySnapshot =
    std::vector<GsTeamBirthdayDirectoryEntry>;

} // namespace ClassMngr::Next::Application
