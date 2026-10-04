#pragma once

#include "next/application/roster_print_class_info_read_snapshot.h"
#include "next/application/roster_snapshot.h"

#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

struct RosterTemplatePrintSourceReadEntry final
{
    Domain::ClassId classId;
    RosterPrintClassInfoReadSnapshot classInfo;
    RosterSnapshot roster;

    RosterTemplatePrintSourceReadEntry(
        Domain::ClassId value,
        RosterPrintClassInfoReadSnapshot info,
        RosterSnapshot rosterSnapshot
        )
        : classId(std::move(value)),
          classInfo(std::move(info)),
          roster(std::move(rosterSnapshot))
    {
    }
};

using RosterTemplatePrintSourceReadSnapshot =
    std::vector<RosterTemplatePrintSourceReadEntry>;

} // namespace ClassMngr::Next::Application
