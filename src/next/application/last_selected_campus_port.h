#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <optional>

namespace ClassMngr::Next::Application
{

// The application boundary carries the optional typed last-selected campus.
// Persistence and legacy value conversion belong to the outer adapter.
class LastSelectedCampusPort
{
public:
    virtual ~LastSelectedCampusPort() = default;

    [[nodiscard]] virtual std::optional<Domain::CampusId> read()
        const = 0;

    [[nodiscard]] virtual Domain::Result<void> write(
        const std::optional<Domain::CampusId>& campusId
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
