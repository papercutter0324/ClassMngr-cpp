#pragma once

#include <string>

namespace classmngr::engine
{

struct GsTeamMember
{
    int id = -1;
    std::string name;
    std::string koreanName;
    std::string position;
    std::string phoneNumber;
    std::string birthday;
};

} // namespace classmngr::engine
