#pragma once

#include "next/application/calendar_event_delete_port.h"

namespace ClassMngr::Next::Application
{

// Application boundary for deleting one calendar event. Legacy identifier
// conversion and service behavior remain owned by the Platform adapter.
class CalendarEventDeleteUseCase final
{
public:
    [[nodiscard]] static CalendarEventDeleteResult execute(
        CalendarEventDeletePort& deletePort,
        const Domain::CalendarEventId& eventId
        )
    {
        return deletePort.deleteEvent(eventId);
    }
};

} // namespace ClassMngr::Next::Application
