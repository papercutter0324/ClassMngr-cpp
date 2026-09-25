#pragma once

#include "next/domain/calendar_event_timing.h"
#include "next/domain/operation_result.h"

#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace ClassMngr::Next::Application
{

inline constexpr std::size_t
    kCalendarEventSeriesEditMaxRepeatSeriesIdLength = 128;
inline constexpr std::size_t kCalendarEventSeriesEditMaxTitleLength = 255;
inline constexpr std::size_t kCalendarEventSeriesEditMaxDateLength = 10;
inline constexpr std::size_t kCalendarEventSeriesEditIsoDateLength =
    kCalendarEventSeriesEditMaxDateLength;
inline constexpr std::size_t kCalendarEventSeriesEditMaxTimeLength = 5;
inline constexpr std::size_t kCalendarEventSeriesEditMaxEventTypeLength = 64;
inline constexpr std::size_t kCalendarEventSeriesEditMaxTimeStatusLength = 64;

// A bounded, adapter-neutral request for editing the suffix of one repeat
// series. startDate selects the first existing occurrence to edit;
// editedStartDate and editedEndDate describe the new date and duration that
// are propagated to every selected occurrence. Dates and times use canonical
// ISO text so conversion to legacy Qt values stays in the platform adapter.
struct CalendarEventSeriesEditRequest final
{
    std::string repeatSeriesId;
    std::string startDate;
    std::string editedStartDate;
    std::string editedEndDate;
    std::string title;
    std::optional<std::string> startTime;
    std::optional<std::string> endTime;
    bool allDay = false;
    std::string eventType = "Other";
    std::string timeStatus = "Timed";

    [[nodiscard]] Domain::Result<void> validate() const;

    friend bool operator==(
        const CalendarEventSeriesEditRequest&,
        const CalendarEventSeriesEditRequest&
        ) = default;
};

using CalendarEventSeriesEditResult = Domain::Result<void>;
using CalendarEventSeriesEditError = Domain::OperationError;

namespace CalendarEventSeriesEditRequestDetail
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

[[nodiscard]] inline bool isEventType(
    const std::string_view value
    ) noexcept
{
    const std::string_view normalized = trimAscii(value);
    return normalized == "Vacation"
        || normalized == "Holiday"
        || normalized == "Workshop"
        || normalized == "CM"
        || normalized == "Meeting"
        || normalized == "Other";
}

[[nodiscard]] inline bool isTimeStatus(
    const std::string_view value
    ) noexcept
{
    const std::string_view normalized = trimAscii(value);
    return normalized == "Timed"
        || normalized == "Unknown"
        || normalized == "Unconfirmed";
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
        return "Calendar repeat-series edit dates must be valid canonical ISO dates.";
    case Domain::CalendarEventTimingIssue::EndDateBeforeStartDate:
        return "Calendar repeat-series edit end date must not precede its start date.";
    case Domain::CalendarEventTimingIssue::TimesMustBePaired:
        return "Calendar repeat-series edit times must be both absent or both present.";
    case Domain::CalendarEventTimingIssue::InvalidTime:
        return "Calendar repeat-series edit times must be valid canonical 24-hour times.";
    case Domain::CalendarEventTimingIssue::
        AllDayRequiresTimedStatusAndNoTimes:
        return "All-day repeat-series edits require Timed status and no times.";
    case Domain::CalendarEventTimingIssue::TimedRequiresBothTimes:
        return "Timed repeat-series edits require both start and end times.";
    case Domain::CalendarEventTimingIssue::EndTimeMustFollowStartTime:
        return "Calendar repeat-series edit end time must be after its start time.";
    case Domain::CalendarEventTimingIssue::NonTimedStatusRequiresNoTimes:
        return "Unknown or unconfirmed repeat-series edits require no times.";
    }

    return "Calendar repeat-series edit timing is invalid.";
}

} // namespace CalendarEventSeriesEditRequestDetail

[[nodiscard]] inline Domain::Result<void>
validateCalendarEventSeriesEditRequest(
    const CalendarEventSeriesEditRequest& request
    )
{
    using namespace CalendarEventSeriesEditRequestDetail;

    if (!isRequiredText(
            request.repeatSeriesId,
            kCalendarEventSeriesEditMaxRepeatSeriesIdLength
            ))
    {
        return invalid(
            "Calendar repeat-series identifier must be non-blank and bounded."
            );
    }

    if (!isRequiredText(
            request.title,
            kCalendarEventSeriesEditMaxTitleLength
            )
        || !isEventType(request.eventType)
        || request.eventType.size()
            > kCalendarEventSeriesEditMaxEventTypeLength
        || !isTimeStatus(request.timeStatus)
        || request.timeStatus.size()
            > kCalendarEventSeriesEditMaxTimeStatusLength)
    {
        return invalid(
            "Calendar repeat-series edit text fields must be non-blank and bounded."
            );
    }

    if (!Domain::CalendarEventTiming::isCanonicalDate(request.startDate))
    {
        return invalid(
            "Calendar repeat-series edit dates must be valid canonical ISO dates."
            );
    }

    const std::string_view timeStatus = trimAscii(request.timeStatus);
    const Domain::CalendarEventTimeStatus domainTimeStatus =
        timeStatus == "Timed"
            ? Domain::CalendarEventTimeStatus::Timed
            : timeStatus == "Unknown"
                ? Domain::CalendarEventTimeStatus::Unknown
                : Domain::CalendarEventTimeStatus::Unconfirmed;
    const Domain::CalendarEventTiming timing(
        request.editedStartDate,
        request.editedEndDate,
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

inline Domain::Result<void> CalendarEventSeriesEditRequest::validate() const
{
    return validateCalendarEventSeriesEditRequest(*this);
}

// Adapter-neutral repeat-series edit. Legacy service pointers and Qt value
// types remain outside this application boundary.
class CalendarEventSeriesEditPort
{
public:
    virtual ~CalendarEventSeriesEditPort() = default;

    [[nodiscard]] virtual CalendarEventSeriesEditResult
    editRepeatSeriesFromDate(
        const CalendarEventSeriesEditRequest& request
        ) = 0;
};

} // namespace ClassMngr::Next::Application
