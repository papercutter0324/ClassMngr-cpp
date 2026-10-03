#pragma once

#include "next/application/roster_availability_batch_read_snapshot.h"
#include "next/domain/operation_result.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

using RosterAvailabilityBatchReadResult =
    Domain::Result<std::vector<RosterAvailabilityReadSnapshot>>;

class RosterAvailabilityBatchReadPort
{
public:
    virtual ~RosterAvailabilityBatchReadPort() = default;

    [[nodiscard]] virtual RosterAvailabilityBatchReadResult
    readRosterAvailability(
        const std::vector<Domain::ClassId>& classIds,
        const std::vector<std::u16string>& baseColumnNames
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
