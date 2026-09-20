#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

using CalendarEventDeleteResult = Domain::Result<void>;
using CalendarEventDeleteError = Domain::OperationError;

// Adapter-neutral single-event deletion. The caller supplies a typed event
// identifier and receives only an owned success or structured failure; the
// legacy service and its integer representation remain outside this boundary.
class CalendarEventDeletePort
{
public:
    virtual ~CalendarEventDeletePort() = default;

    [[nodiscard]] virtual CalendarEventDeleteResult deleteEvent(
        const Domain::CalendarEventId& eventId
        ) = 0;
};

} // namespace ClassMngr::Next::Application
