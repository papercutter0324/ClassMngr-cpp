#pragma once

#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

using CalendarEventDeleteAllResult = Domain::Result<void>;
using CalendarEventDeleteAllError = Domain::OperationError;

// Adapter-neutral destructive operation for clearing the active calendar.
// Availability is exposed separately so the UI can preserve its existing
// behavior of hiding the reset confirmation when no workspace is open.
class CalendarEventDeleteAllPort
{
public:
    virtual ~CalendarEventDeleteAllPort() = default;

    [[nodiscard]] virtual bool isAvailable() const = 0;

    [[nodiscard]] virtual CalendarEventDeleteAllResult deleteAllEvents() = 0;
};

} // namespace ClassMngr::Next::Application
