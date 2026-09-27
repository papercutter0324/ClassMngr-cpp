#pragma once

#include "next/application/calendar_event_series_delete_port.h"

namespace ClassMngr::Next::Application
{

// Validate the selected repeat-series suffix before crossing the platform
// boundary. The platform adapter retains the same check for direct callers.
class CalendarEventSeriesDeleteUseCase final
{
public:
    [[nodiscard]] static CalendarEventSeriesDeleteResult execute(
        CalendarEventSeriesDeletePort& deletePort,
        const CalendarEventSeriesDeleteRequest& request
        )
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return CalendarEventSeriesDeleteResult::failure(
                validation.error()
                );
        }

        return deletePort.deleteRepeatSeriesFromDate(request);
    }
};

} // namespace ClassMngr::Next::Application
