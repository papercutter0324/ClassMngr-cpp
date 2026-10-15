#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

// Successful firstEmptyRow values are in [0, 24], or -1 when full.
struct RosterAvailabilityReadSnapshot final
{
    Domain::ClassId classId;
    Domain::Result<int> firstEmptyRowResult =
        Domain::Result<int>::success(0);

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
