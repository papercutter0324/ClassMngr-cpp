#pragma once

#include "next/domain/domain_types.h"

namespace ClassMngr::Next::Application
{

// firstEmptyRow is in [0, 24], or -1 when the modeled roster is full.
struct RosterAvailabilityReadSnapshot final
{
    Domain::ClassId classId;
    int firstEmptyRow = 0;

    friend bool operator==(
        const RosterAvailabilityReadSnapshot&,
        const RosterAvailabilityReadSnapshot&
        ) = default;
};

} // namespace ClassMngr::Next::Application
