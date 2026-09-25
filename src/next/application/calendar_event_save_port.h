#pragma once

#include "next/domain/calendar_event_timing.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace ClassMngr::Next::Application
{

inline constexpr std::size_t kCalendarEventSaveMaxIdentifierLength = 256;
inline constexpr std::size_t kCalendarEventSaveMaxTitleLength = 255;
inline constexpr std::size_t kCalendarEventSaveMaxDateLength = 10;
inline constexpr std::size_t kCalendarEventSaveMaxTimeLength = 5;
inline constexpr std::size_t kCalendarEventSaveMaxEventTypeLength = 64;
inline constexpr std::size_t kCalendarEventSaveMaxTimeStatusLength = 64;

// A bounded, adapter-neutral request for saving one non-repeat calendar
// event. An absent identifier requests creation; a present identifier requests
// an update. Dates and times use canonical ISO text so platform conversion
// stays at the Qt/legacy boundary.
struct CalendarEventSaveRequest final
{
    std::optional<Domain::CalendarEventId> id;
    std::string title;
    std::string startDate;
    std::string endDate;
    std::optional<std::string> startTime;
    std::optional<std::string> endTime;
    bool allDay = false;
    std::string eventType = "Other";
    std::string timeStatus = "Timed";

    [[nodiscard]] Domain::Result<void> validate() const;

    friend bool operator==(
        const CalendarEventSaveRequest&,
        const CalendarEventSaveRequest&
        ) = default;
};

using CalendarEventSaveResult = Domain::Result<Domain::CalendarEventId>;
using CalendarEventSaveError = Domain::OperationError;

namespace CalendarEventSaveRequestDetail
{

[[nodiscard]] inline bool isBlank(
    const std::string_view value
    ) noexcept
{
    return value.empty()
        || [&value]()
        {
            for (const unsigned char character : value)
            {
                if (std::isspace(character) == 0)
                {
                    return false;
                }
            }

            return true;
        }();
}

[[nodiscard]] inline std::string_view trimAscii(
    const std::string_view value
    ) noexcept
{
    std::size_t first = 0;
    while (first < value.size()
           && std::isspace(
               static_cast<unsigned char>(value.at(first))
               ) != 0)
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first
           && std::isspace(
               static_cast<unsigned char>(value.at(last - 1))
               ) != 0)
    {
        --last;
    }

    return value.substr(first, last - first);
}

[[nodiscard]] inline bool isRequiredText(
    const std::string_view value,
    const std::size_t maximumLength
    ) noexcept
{
    return !isBlank(value) && value.size() <= maximumLength;
}

[[nodiscard]] inline bool isBoundedIdentifier(
    const Domain::CalendarEventId& id
    ) noexcept
{
    const std::string& value = id.value();
    return !isBlank(value)
        && value.size() <= kCalendarEventSaveMaxIdentifierLength;
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

[[nodiscard]] inline const char* calendarEventTimingIssueMessage(
    const Domain::CalendarEventTimingIssue issue
    ) noexcept
{
    switch (issue)
    {
    case Domain::CalendarEventTimingIssue::InvalidDate:
        return "Calendar event dates must be valid canonical ISO dates.";
    case Domain::CalendarEventTimingIssue::EndDateBeforeStartDate:
        return "Calendar event end date must not precede its start date.";
    case Domain::CalendarEventTimingIssue::TimesMustBePaired:
        return "Calendar event start and end times must be both absent or both present.";
    case Domain::CalendarEventTimingIssue::InvalidTime:
        return "Calendar event times must be valid canonical 24-hour times.";
    case Domain::CalendarEventTimingIssue::
        AllDayRequiresTimedStatusAndNoTimes:
        return "All-day calendar events require Timed status and no times.";
    case Domain::CalendarEventTimingIssue::TimedRequiresBothTimes:
        return "Timed calendar events require both start and end times.";
    case Domain::CalendarEventTimingIssue::EndTimeMustFollowStartTime:
        return "Calendar event end time must be after its start time.";
    case Domain::CalendarEventTimingIssue::NonTimedStatusRequiresNoTimes:
        return "Unknown or unconfirmed calendar events require no times.";
    }

    return "Calendar event timing is invalid.";
}

} // namespace CalendarEventSaveRequestDetail

[[nodiscard]] inline Domain::Result<void> validateCalendarEventSaveRequest(
    const CalendarEventSaveRequest& request
    )
{
    using namespace CalendarEventSaveRequestDetail;

    if (request.id.has_value() && !isBoundedIdentifier(*request.id))
    {
        return invalid(
            "Calendar event identifier must be non-blank and bounded."
            );
    }

    if (!isRequiredText(
            request.title,
            kCalendarEventSaveMaxTitleLength
            )
        || !Domain::calendarEventTypeFromName(
                trimAscii(request.eventType)
                ).has_value()
        || request.eventType.size() > kCalendarEventSaveMaxEventTypeLength
        || !Domain::calendarEventTimeStatusFromName(
                trimAscii(request.timeStatus)
                ).has_value()
        || request.timeStatus.size() > kCalendarEventSaveMaxTimeStatusLength)
    {
        return invalid(
            "Calendar event text fields must be non-blank and bounded."
            );
    }

    const Domain::CalendarEventTimeStatus domainTimeStatus =
        Domain::calendarEventTimeStatusFromName(
            trimAscii(request.timeStatus)
            ).value();
    const Domain::CalendarEventTiming timing(
        request.startDate,
        request.endDate,
        request.startTime,
        request.endTime,
        request.allDay,
        domainTimeStatus
        );
    if (const auto issue = timing.validate())
    {
        return invalid(calendarEventTimingIssueMessage(*issue));
    }

    return Domain::Result<void>::success();
}

inline Domain::Result<void> CalendarEventSaveRequest::validate() const
{
    return validateCalendarEventSaveRequest(*this);
}

// Adapter-neutral single-event save. The legacy service and its Qt value
// types remain outside this application boundary.
class CalendarEventSavePort
{
public:
    virtual ~CalendarEventSavePort() = default;

    [[nodiscard]] virtual CalendarEventSaveResult saveEvent(
        const CalendarEventSaveRequest& request
        ) = 0;
};

} // namespace ClassMngr::Next::Application
