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

        const auto eventId = Domain::CalendarEventId::fromString(
            request.eventId
            );
        if (!eventId)
        {
            return CalendarEventByIdQueryResult::failure(
                CalendarEventByIdQueryRequestDetail::invalidRequest().error()
                );
        }

        CalendarEventByIdQueryResult result =
            queryPort.loadEventById(*eventId);
        if (!result)
        {
            return CalendarEventByIdQueryResult::failure(result.error());
        }

        if (result.value().id != *eventId)
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
