#pragma once

#include "next/application/roster_column_names_batch_read_snapshot.h"
#include "next/domain/operation_result.h"

#include <vector>

namespace ClassMngr::Next::Application
{

using RosterColumnNamesBatchReadResult =
    Domain::Result<std::vector<RosterColumnNamesReadSnapshot>>;

class RosterColumnNamesBatchReadPort
{
public:
    virtual ~RosterColumnNamesBatchReadPort() = default;

    [[nodiscard]] virtual RosterColumnNamesBatchReadResult
    readRosterColumnNames(
        const std::vector<Domain::ClassId>& classIds
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
