#pragma once

#include "next/application/roster_template_print_source_read_snapshot.h"
#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

struct RosterTemplatePrintSourceReadRequest final
{
    std::vector<Domain::ClassId> classIds;
};

using RosterTemplatePrintSourceReadResult =
    Domain::Result<RosterTemplatePrintSourceReadSnapshot>;

class RosterTemplatePrintSourceReadPort
{
public:
    virtual ~RosterTemplatePrintSourceReadPort() = default;

    [[nodiscard]] virtual RosterTemplatePrintSourceReadResult
    readRosterTemplatePrintSource(
        const RosterTemplatePrintSourceReadRequest& query
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
