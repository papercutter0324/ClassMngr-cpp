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
        const CalendarEventSaveRequest& request
        )
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return CalendarEventSaveResult::failure(validation.error());
        }

        return savePort.saveEvent(request);
    }
};

} // namespace ClassMngr::Next::Application
