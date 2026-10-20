#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"
#include "next/application/roster_availability_first_empty_row.h"

namespace ClassMngr::Next::Application
{

// Successful firstEmptyRow values are in [0, 24], or the full sentinel.
struct RosterAvailabilityReadSnapshot final
{
    Domain::ClassId classId;
    Domain::Result<RosterAvailabilityFirstEmptyRow> firstEmptyRowResult =
        Domain::Result<RosterAvailabilityFirstEmptyRow>::success(
            RosterAvailabilityFirstEmptyRow{}
            );

    friend bool operator==(
        const RosterAvailabilityReadSnapshot& left,
        const RosterAvailabilityReadSnapshot& right
        )
    {
        if (left.classId != right.classId
            || static_cast<bool>(left.firstEmptyRowResult)
                != static_cast<bool>(right.firstEmptyRowResult))
        {
            return false;
        }
        if (left.firstEmptyRowResult)
        {
            return left.firstEmptyRowResult.value()
                == right.firstEmptyRowResult.value();
        }
        return left.firstEmptyRowResult.error()
            == right.firstEmptyRowResult.error();
    }
};

} // namespace ClassMngr::Next::Application
