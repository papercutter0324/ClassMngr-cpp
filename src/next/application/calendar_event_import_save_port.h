#pragma once

#include "next/application/calendar_event_save_port.h"

#include <cstddef>
#include <vector>

namespace ClassMngr::Next::Application
{

inline constexpr std::size_t kCalendarEventImportSaveMaxEvents = 4096;

// One ordered calendar-import batch. The importer only creates events, so
// each save request must omit an existing identifier. Empty batches are valid
// for a duplicate-only import that has nothing to persist.
struct CalendarEventImportSaveRequest final
{
    std::vector<CalendarEventSaveRequest> events;

    [[nodiscard]] Domain::Result<void> validate() const;

    friend bool operator==(
        const CalendarEventImportSaveRequest&,
        const CalendarEventImportSaveRequest&
        ) = default;
};

using CalendarEventImportSaveResult =
    Domain::Result<std::vector<Domain::CalendarEventId>>;
using CalendarEventImportSaveError = Domain::OperationError;

namespace CalendarEventImportSaveRequestDetail
{

[[nodiscard]] inline Domain::Result<void> invalid(
    const char* message
    )
{
    return Domain::Result<void>::failure({
        .code = Domain::ErrorCode::InvalidInput,
        .message = message,
        .recoverable = false
    });
}

} // namespace CalendarEventImportSaveRequestDetail

[[nodiscard]] inline Domain::Result<void>
validateCalendarEventImportSaveRequest(
    const CalendarEventImportSaveRequest& request
    )
{
    using namespace CalendarEventImportSaveRequestDetail;

    if (request.events.size() > kCalendarEventImportSaveMaxEvents)
    {
        return invalid(
            "Calendar import event count exceeds the maximum."
            );
    }

    for (const CalendarEventSaveRequest& event : request.events)
    {
        if (event.id.has_value())
        {
            return invalid(
                "Calendar imports may only create new events."
                );
        }

        const Domain::Result<void> eventValidation = event.validate();
        if (!eventValidation)
        {
            return eventValidation;
        }
    }

    return Domain::Result<void>::success();
}

inline Domain::Result<void> CalendarEventImportSaveRequest::validate() const
{
    return validateCalendarEventImportSaveRequest(*this);
}

// Persists one ordered import batch. The adapter owns the legacy transaction
// boundary and returns the created identifiers in the same order.
class CalendarEventImportSavePort
{
public:
    virtual ~CalendarEventImportSavePort() = default;

    [[nodiscard]] virtual CalendarEventImportSaveResult saveImportedEvents(
        const CalendarEventImportSaveRequest& request
        ) = 0;
};

} // namespace ClassMngr::Next::Application
