#pragma once

#include "next/application/calendar_event_projection.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <cctype>
#include <cstddef>
#include <string>

namespace ClassMngr::Next::Application
{

inline constexpr std::size_t kCalendarEventByIdQueryMaxIdentifierLength =
    kCalendarEventSummaryMaxIdentifierLength;

struct CalendarEventByIdQueryRequest final
{
    std::string eventId;

    [[nodiscard]] Domain::Result<void> validate() const;

    friend bool operator==(
        const CalendarEventByIdQueryRequest&,
        const CalendarEventByIdQueryRequest&
        ) = default;
};

using CalendarEventByIdQueryResult = Domain::Result<CalendarEventSummary>;
using CalendarEventByIdQueryError = Domain::OperationError;

namespace CalendarEventByIdQueryRequestDetail
{

[[nodiscard]] inline bool isBlank(const std::string& value) noexcept
{
    if (value.empty())
    {
        return true;
    }

    for (const unsigned char character : value)
    {
        if (std::isspace(character) == 0)
        {
            return false;
        }
    }

    return true;
}

[[nodiscard]] inline Domain::Result<void> invalidRequest()
{
    return Domain::Result<void>::failure({
        .code = Domain::ErrorCode::InvalidInput,
        .message = "Calendar event identifier must be non-blank and bounded.",
        .recoverable = false
    });
}

} // namespace CalendarEventByIdQueryRequestDetail

[[nodiscard]] inline Domain::Result<void>
validateCalendarEventByIdQueryRequest(
    const CalendarEventByIdQueryRequest& request
    )
{
    if (CalendarEventByIdQueryRequestDetail::isBlank(request.eventId)
        || request.eventId.size()
            > kCalendarEventByIdQueryMaxIdentifierLength
        || !Domain::CalendarEventId::fromString(request.eventId).has_value())
    {
        return CalendarEventByIdQueryRequestDetail::invalidRequest();
    }

    return Domain::Result<void>::success();
}

inline Domain::Result<void> CalendarEventByIdQueryRequest::validate() const
{
    return validateCalendarEventByIdQueryRequest(*this);
}

// Loads one bounded, application-owned summary for a stable event ID.
class CalendarEventByIdQueryPort
{
public:
    virtual ~CalendarEventByIdQueryPort() = default;

    [[nodiscard]] virtual CalendarEventByIdQueryResult loadEventById(
        const Domain::CalendarEventId& eventId
        ) = 0;
};

} // namespace ClassMngr::Next::Application
