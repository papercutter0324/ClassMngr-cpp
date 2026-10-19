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
        const CalendarEventSeriesEditInput& input
        )
    {
        const Domain::Result<CalendarEventSeriesEditRequest> request =
            input.toRequest();
        if (!request)
        {
            return CalendarEventSeriesEditResult::failure(request.error());
        }

        return editPort.editRepeatSeriesFromDate(request.value());
    }
};

} // namespace ClassMngr::Next::Application
