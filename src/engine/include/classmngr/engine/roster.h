#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace classmngr::engine
{

inline constexpr std::array<std::string_view, 6> RosterBaseColumns{
    "English",
    "Korean",
    "Winter",
    "Speech Contest",
    "Summer",
    "Fall"
};

struct Roster
{
    std::vector<std::string> columns;
    std::vector<int> columnWidths;
    std::vector<std::vector<std::string>> rows;
};

} // namespace classmngr::engine
