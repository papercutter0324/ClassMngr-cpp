#pragma once

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

[[nodiscard]] inline bool isCanonicalDate(
    const std::string_view value
    ) noexcept
{
    if (value.size() != kCalendarEventSeriesEditMaxDateLength)
    {
        return false;
    }

    for (std::size_t index = 0; index < value.size(); ++index)
    {
        if ((index == 4 || index == 7) && value.at(index) == '-')
        {
            continue;
        }

        if (value.at(index) < '0' || value.at(index) > '9')
        {
            return false;
        }
    }

    const int year =
        (value.at(0) - '0') * 1000
        + (value.at(1) - '0') * 100
        + (value.at(2) - '0') * 10
        + (value.at(3) - '0');
    const int month =
        (value.at(5) - '0') * 10
        + (value.at(6) - '0');
    const int day =
        (value.at(8) - '0') * 10
        + (value.at(9) - '0');
    if (year <= 0 || month < 1 || month > 12 || day < 1)
    {
        return false;
    }

    constexpr int daysInMonth[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };
    int maximumDay = daysInMonth[month - 1];
    if (month == 2
        && (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)))
    {
        maximumDay = 29;
    }

    return day <= maximumDay;
}

[[nodiscard]] inline bool isCanonicalTime(
    const std::string_view value
    ) noexcept
{
    if (value.size() != kCalendarEventSeriesEditMaxTimeLength
        || value.at(2) != ':')
    {
        return false;
    }

    for (const std::size_t index : {0U, 1U, 3U, 4U})
    {
        if (value.at(index) < '0' || value.at(index) > '9')
        {
            return false;
        }
    }

    const int hour =
        (value.at(0) - '0') * 10 + value.at(1) - '0';
    const int minute =
        (value.at(3) - '0') * 10 + value.at(4) - '0';
    return hour <= 23 && minute <= 59;
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

    if (!isCanonicalDate(request.startDate)
        || !isCanonicalDate(request.editedStartDate)
        || !isCanonicalDate(request.editedEndDate))
    {
        return invalid(
            "Calendar repeat-series edit dates must be valid canonical ISO dates."
            );
    }

    if (request.editedEndDate < request.editedStartDate)
    {
        return invalid(
            "Calendar repeat-series edit end date must not precede its start date."
            );
    }

    const std::string_view timeStatus = trimAscii(request.timeStatus);
    const bool hasStartTime = request.startTime.has_value();
    const bool hasEndTime = request.endTime.has_value();
    if (hasStartTime != hasEndTime)
    {
        return invalid(
            "Calendar repeat-series edit times must be both absent or both present."
            );
    }

    if ((hasStartTime && !isCanonicalTime(*request.startTime))
        || (hasEndTime && !isCanonicalTime(*request.endTime)))
    {
        return invalid(
            "Calendar repeat-series edit times must be valid canonical 24-hour times."
            );
    }

    if (request.allDay)
    {
        if (timeStatus != "Timed" || hasStartTime || hasEndTime)
        {
            return invalid(
                "All-day repeat-series edits require Timed status and no times."
                );
        }

        return Domain::Result<void>::success();
    }

    if (timeStatus == "Timed")
    {
        if (!hasStartTime || !hasEndTime)
        {
            return invalid(
                "Timed repeat-series edits require both start and end times."
                );
        }

        if (request.editedStartDate == request.editedEndDate
            && *request.endTime <= *request.startTime)
        {
            return invalid(
                "Calendar repeat-series edit end time must be after its start time."
                );
        }
    }
    else if (hasStartTime || hasEndTime)
    {
        return invalid(
            "Unknown or unconfirmed repeat-series edits require no times."
            );
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
