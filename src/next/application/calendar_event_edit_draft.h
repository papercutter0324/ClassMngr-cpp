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

inline constexpr std::size_t kCalendarEventEditDraftMaxIdentifierLength =
    256;
inline constexpr std::size_t
    kCalendarEventEditDraftMaxRepeatSeriesIdLength = 128;
inline constexpr std::size_t kCalendarEventEditDraftMaxTitleLength = 255;
inline constexpr std::size_t kCalendarEventEditDraftMaxDateLength = 10;
inline constexpr std::size_t kCalendarEventEditDraftMaxTimeLength = 5;
inline constexpr std::size_t kCalendarEventEditDraftMaxEventTypeLength = 64;
inline constexpr std::size_t kCalendarEventEditDraftMaxTimeStatusLength = 64;

// A bounded, value-like snapshot of the fields edited by the calendar event
// dialog. The dialog owns conversion from its legacy Qt value; consumers can
// map this draft to typed application requests without taking a Qt dependency.
struct CalendarEventEditDraft final
{
    std::optional<Domain::CalendarEventId> id;
    std::optional<std::string> repeatSeriesId;
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
        const CalendarEventEditDraft&,
        const CalendarEventEditDraft&
        ) = default;
};

namespace CalendarEventEditDraftDetail
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

[[nodiscard]] inline bool isBoundedIdentifier(
    const Domain::CalendarEventId& id
    ) noexcept
{
    const std::string& value = id.value();
    return isRequiredText(value, kCalendarEventEditDraftMaxIdentifierLength);
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
        return "Calendar event draft dates must be valid canonical ISO dates.";
    case Domain::CalendarEventTimingIssue::EndDateBeforeStartDate:
        return "Calendar event draft end date must not precede its start date.";
    case Domain::CalendarEventTimingIssue::TimesMustBePaired:
        return "Calendar event draft start and end times must be both absent or both present.";
    case Domain::CalendarEventTimingIssue::InvalidTime:
        return "Calendar event draft times must be valid canonical 24-hour times.";
    case Domain::CalendarEventTimingIssue::
        AllDayRequiresTimedStatusAndNoTimes:
        return "All-day calendar event drafts require Timed status and no times.";
    case Domain::CalendarEventTimingIssue::TimedRequiresBothTimes:
        return "Timed calendar event drafts require both start and end times.";
    case Domain::CalendarEventTimingIssue::EndTimeMustFollowStartTime:
        return "Calendar event draft end time must be after its start time.";
    case Domain::CalendarEventTimingIssue::NonTimedStatusRequiresNoTimes:
        return "Unknown or unconfirmed calendar event drafts require no times.";
    }

    return "Calendar event draft timing is invalid.";
}

} // namespace CalendarEventEditDraftDetail

[[nodiscard]] inline Domain::Result<void>
validateCalendarEventEditDraft(
    const CalendarEventEditDraft& draft
    )
{
    using namespace CalendarEventEditDraftDetail;

    if (draft.id.has_value() && !isBoundedIdentifier(*draft.id))
    {
        return invalid(
            "Calendar event identifier must be non-blank and bounded."
            );
    }

    if (draft.repeatSeriesId.has_value()
        && !isRequiredText(
            *draft.repeatSeriesId,
            kCalendarEventEditDraftMaxRepeatSeriesIdLength
            ))
    {
        return invalid(
            "Calendar repeat-series identifier must be non-blank and bounded."
            );
    }

    if (!isRequiredText(
            draft.title,
            kCalendarEventEditDraftMaxTitleLength
            )
        || !isEventType(draft.eventType)
        || draft.eventType.size() > kCalendarEventEditDraftMaxEventTypeLength
        || !isTimeStatus(draft.timeStatus)
        || draft.timeStatus.size()
            > kCalendarEventEditDraftMaxTimeStatusLength)
    {
        return invalid(
            "Calendar event draft text fields must be non-blank and bounded."
            );
    }

    const std::string_view timeStatus = trimAscii(draft.timeStatus);
    const Domain::CalendarEventTimeStatus domainTimeStatus =
        timeStatus == "Timed"
            ? Domain::CalendarEventTimeStatus::Timed
            : timeStatus == "Unknown"
                ? Domain::CalendarEventTimeStatus::Unknown
                : Domain::CalendarEventTimeStatus::Unconfirmed;
    const Domain::CalendarEventTiming timing(
        draft.startDate,
        draft.endDate,
        draft.startTime,
        draft.endTime,
        draft.allDay,
        domainTimeStatus
        );
    if (const auto issue = timing.validate())
    {
        return invalid(calendarEventTimingIssueMessage(*issue));
    }

    return Domain::Result<void>::success();
}

inline Domain::Result<void> CalendarEventEditDraft::validate() const
{
    return validateCalendarEventEditDraft(*this);
}

} // namespace ClassMngr::Next::Application
