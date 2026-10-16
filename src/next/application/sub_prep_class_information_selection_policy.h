#pragma once

#include "next/domain/domain_types.h"

#include <algorithm>
#include <optional>
#include <vector>

namespace ClassMngr::Next::Application
{

// Keeps the current class when it remains among visible rows; otherwise it
// selects the first row in the caller-provided model order.
[[nodiscard]] inline std::optional<Domain::ClassId>
selectSubPrepClassInformationId(
    const std::optional<Domain::ClassId>& selectedClassId,
    const std::vector<Domain::ClassId>& orderedVisibleClassIds
    )
{
    if (
        selectedClassId.has_value()
        && std::find(
               orderedVisibleClassIds.cbegin(),
               orderedVisibleClassIds.cend(),
               *selectedClassId
               ) != orderedVisibleClassIds.cend()
        )
    {
        return selectedClassId;
    }

    if (orderedVisibleClassIds.empty())
    {
        return std::nullopt;
    }

    return orderedVisibleClassIds.front();
}

} // namespace ClassMngr::Next::Application
