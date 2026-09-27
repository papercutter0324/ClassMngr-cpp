#pragma once

#include "next/application/calendar_event_series_edit_port.h"

namespace ClassMngr::Next::Application
{

// Validate repeat-series edit input before crossing the persistence boundary.
// Platform adapters retain their own guard for direct callers.
class CalendarEventSeriesEditUseCase final
{
public:
    [[nodiscard]] static CalendarEventSeriesEditResult execute(
        CalendarEventSeriesEditPort& editPort,
        const CalendarEventSeriesEditRequest& request
        )
    {
        const CalendarEventSeriesEditResult validation = request.validate();
        if (!validation)
        {
            return validation;
        }

        return editPort.editRepeatSeriesFromDate(request);
    }
};

} // namespace ClassMngr::Next::Application
