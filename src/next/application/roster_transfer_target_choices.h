#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"
#include "next/application/roster_transfer_target_display_name.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct RosterTransferTargetChoice final
{
    Domain::ClassId classId;
    std::u16string className;
    RosterTransferTargetDisplayMetadata displayMetadata;
    bool full = false;
};

struct RosterTransferTargetChoicesSnapshot final
{
    std::u16string sourceGrade;
    std::vector<RosterTransferTargetChoice> candidates;
};

using RosterTransferTargetChoicesResult =
    Domain::Result<RosterTransferTargetChoicesSnapshot>;

} // namespace ClassMngr::Next::Application
