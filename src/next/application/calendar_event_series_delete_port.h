#pragma once

#include "next/domain/calendar_event_timing.h"
#include "next/domain/operation_result.h"

#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

namespace ClassMngr::Next::Application
{

inline constexpr std::size_t
    kCalendarEventSeriesDeleteMaxRepeatSeriesIdLength = 128;
inline constexpr std::size_t kCalendarEventSeriesDeleteIsoDateLength = 10;

// A bounded, adapter-neutral request for deleting one repeat-series suffix.
// The date is canonical ISO-8601 calendar text (yyyy-MM-dd); the Application
// contract validates it and the platform adapter converts it to a native date.
struct CalendarEventSeriesDeleteRequest final
{
    std::string repeatSeriesId;
    std::string startDate;

    [[nodiscard]] Domain::Result<void> validate() const;

    friend bool operator==(
        const CalendarEventSeriesDeleteRequest&,
        const CalendarEventSeriesDeleteRequest&
        ) = default;
};

namespace CalendarEventSeriesDeleteRequestDetail
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

} // namespace CalendarEventSeriesDeleteRequestDetail

[[nodiscard]] inline Domain::Result<void>
validateCalendarEventSeriesDeleteRequest(
    const CalendarEventSeriesDeleteRequest& request
    )
{
    using CalendarEventSeriesDeleteRequestDetail::isBlank;

    if (request.repeatSeriesId.empty()
        || request.repeatSeriesId.size()
            > kCalendarEventSeriesDeleteMaxRepeatSeriesIdLength
        || isBlank(request.repeatSeriesId)
        || !Domain::CalendarEventTiming::isCanonicalDate(request.startDate))
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message =
                "Calendar repeat-series delete request must contain a "
                "non-blank bounded series identifier and a valid ISO start "
                "date.",
            .recoverable = false
        });
    }

    return Domain::Result<void>::success();
}

inline Domain::Result<void>
CalendarEventSeriesDeleteRequest::validate() const
{
    return validateCalendarEventSeriesDeleteRequest(*this);
}

using CalendarEventSeriesDeleteResult = Domain::Result<void>;
using CalendarEventSeriesDeleteError = Domain::OperationError;

// Adapter-neutral repeat-series deletion. Legacy service pointers and Qt date
// values remain outside this application boundary.
class CalendarEventSeriesDeletePort
{
public:
    virtual ~CalendarEventSeriesDeletePort() = default;

    [[nodiscard]] virtual CalendarEventSeriesDeleteResult
    deleteRepeatSeriesFromDate(
        const CalendarEventSeriesDeleteRequest& request
        ) = 0;
};

} // namespace ClassMngr::Next::Application
