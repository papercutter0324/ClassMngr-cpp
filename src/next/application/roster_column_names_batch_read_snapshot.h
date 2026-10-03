#pragma once

#include "next/domain/domain_types.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct RosterColumnNamesReadSnapshot final
{
    Domain::ClassId classId;
    std::vector<std::u16string> columns;

    friend bool operator==(
        const RosterColumnNamesReadSnapshot&,
        const RosterColumnNamesReadSnapshot&
        ) = default;
};

} // namespace ClassMngr::Next::Application
