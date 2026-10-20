#pragma once

#include "next/application/calendar_event_save_port.h"

namespace ClassMngr::Next::Application
{

// Validate a single-event save before crossing the persistence boundary.
// Repeat-series operations use their dedicated application ports.
class CalendarEventSaveUseCase final
{
public:
    [[nodiscard]] static CalendarEventSaveResult execute(
        CalendarEventSavePort& savePort,
        const CalendarEventSaveInput& input
        )
    {
        const Domain::Result<CalendarEventSaveRequest> request =
            input.toRequest();
        if (!request)
        {
            return CalendarEventSaveResult::failure(request.error());
        }

        return savePort.saveEvent(request.value());
    }
};

} // namespace ClassMngr::Next::Application
