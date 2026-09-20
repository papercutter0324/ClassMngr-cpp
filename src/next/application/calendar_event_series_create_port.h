#pragma once

#include "next/application/calendar_event_save_port.h"

#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ClassMngr::Next::Application
{

inline constexpr std::size_t
    kCalendarEventSeriesCreateMaxRepeatSeriesIdLength = 128;
inline constexpr std::size_t kCalendarEventSeriesCreateMaxOccurrences = 366;

// A bounded, adapter-neutral request for creating one repeat series. Each
// occurrence reuses the typed single-event save shape; the series identifier
// is supplied once and applied by the platform adapter to every occurrence.
struct CalendarEventSeriesCreateRequest final
{
    std::string repeatSeriesId;
    std::vector<CalendarEventSaveRequest> occurrences;

    [[nodiscard]] Domain::Result<void> validate() const;

    friend bool operator==(
        const CalendarEventSeriesCreateRequest&,
        const CalendarEventSeriesCreateRequest&
        ) = default;
};

using CalendarEventSeriesCreateResult =
    Domain::Result<std::vector<Domain::CalendarEventId>>;
using CalendarEventSeriesCreateError = Domain::OperationError;

namespace CalendarEventSeriesCreateRequestDetail
{

[[nodiscard]] inline bool isBlank(
    const std::string_view value
    ) noexcept
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

} // namespace CalendarEventSeriesCreateRequestDetail

[[nodiscard]] inline Domain::Result<void>
validateCalendarEventSeriesCreateRequest(
    const CalendarEventSeriesCreateRequest& request
    )
{
    using namespace CalendarEventSeriesCreateRequestDetail;

    if (isBlank(request.repeatSeriesId)
        || request.repeatSeriesId.size()
            > kCalendarEventSeriesCreateMaxRepeatSeriesIdLength)
    {
        return invalid(
            "Calendar repeat-series identifier must be non-blank and bounded."
            );
    }

    if (request.occurrences.empty())
    {
        return invalid(
            "Calendar repeat series must contain at least one occurrence."
            );
    }

    if (request.occurrences.size() > kCalendarEventSeriesCreateMaxOccurrences)
    {
        return invalid(
            "Calendar repeat series occurrence count exceeds the maximum."
            );
    }

    for (const CalendarEventSaveRequest& occurrence : request.occurrences)
    {
        const Domain::Result<void> occurrenceValidation =
            occurrence.validate();
        if (!occurrenceValidation)
        {
            return occurrenceValidation;
        }
    }

    return Domain::Result<void>::success();
}

inline Domain::Result<void>
CalendarEventSeriesCreateRequest::validate() const
{
    return validateCalendarEventSeriesCreateRequest(*this);
}

// Adapter-neutral repeat-series creation. Legacy service pointers, Qt value
// types, and batch-container details remain outside this application boundary.
class CalendarEventSeriesCreatePort
{
public:
    virtual ~CalendarEventSeriesCreatePort() = default;

    [[nodiscard]] virtual CalendarEventSeriesCreateResult
    createRepeatSeries(
        const CalendarEventSeriesCreateRequest& request
        ) = 0;
};

} // namespace ClassMngr::Next::Application
