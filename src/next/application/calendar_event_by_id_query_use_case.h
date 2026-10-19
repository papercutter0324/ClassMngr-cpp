#pragma once

#include "next/application/calendar_event_by_id_query_port.h"

namespace ClassMngr::Next::Application
{

// Validate a bounded event identifier before crossing the query boundary.
class CalendarEventByIdQueryUseCase final
{
public:
    [[nodiscard]] static CalendarEventByIdQueryResult execute(
        CalendarEventByIdQueryPort& queryPort,
        const CalendarEventByIdQueryRequest& request
        )
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return CalendarEventByIdQueryResult::failure(
                validation.error()
                );
        }

        CalendarEventByIdQueryResult result =
            queryPort.loadEventById(*request.eventId);
        if (!result)
        {
            return CalendarEventByIdQueryResult::failure(result.error());
        }

        if (result.value().id != *request.eventId)
        {
            return CalendarEventByIdQueryResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "Calendar event lookup returned a different event identifier.",
                .recoverable = false
            });
        }

        return result;
    }
};

} // namespace ClassMngr::Next::Application
