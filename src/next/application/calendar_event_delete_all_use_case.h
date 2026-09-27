#pragma once

#include "next/application/calendar_event_delete_all_port.h"

namespace ClassMngr::Next::Application
{

// Preserve the separate availability and delete operations used by the
// preferences UI while keeping service and error mapping in Platform.
class CalendarEventDeleteAllUseCase final
{
public:
    [[nodiscard]] static bool isAvailable(
        const CalendarEventDeleteAllPort& deleteAllPort
        )
    {
        return deleteAllPort.isAvailable();
    }

    [[nodiscard]] static CalendarEventDeleteAllResult execute(
        CalendarEventDeleteAllPort& deleteAllPort
        )
    {
        return deleteAllPort.deleteAllEvents();
    }
};

} // namespace ClassMngr::Next::Application
