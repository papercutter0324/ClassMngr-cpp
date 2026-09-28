#pragma once

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct RosterSnapshot final
{
    std::vector<std::u16string> columns;
    std::vector<int> columnWidths;
    std::vector<std::vector<std::u16string>> rows;

    friend bool operator==(
        const RosterSnapshot&,
        const RosterSnapshot&
        ) = default;
};

} // namespace ClassMngr::Next::Application
