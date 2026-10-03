#pragma once

#include "next/application/roster_print_class_info_read_snapshot.h"
#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

using RosterPrintClassInfoReadResult =
    Domain::Result<RosterPrintClassInfoReadSnapshot>;

class RosterPrintClassInfoReadPort
{
public:
    virtual ~RosterPrintClassInfoReadPort() = default;

    [[nodiscard]] virtual RosterPrintClassInfoReadResult
    readRosterPrintClassInfo(
        const Domain::ClassId& classId
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
