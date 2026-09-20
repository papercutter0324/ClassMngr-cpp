#pragma once

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

[[nodiscard]] inline bool isCanonicalDate(
    const std::string_view value
    ) noexcept
{
    if (value.size() != kCalendarEventSaveMaxDateLength)
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
    if (value.size() != kCalendarEventSaveMaxTimeLength
        || value.at(2) != ':')
    {
        return false;
    }

    for (std::size_t index : {0U, 1U, 3U, 4U})
    {
        if (value.at(index) < '0' || value.at(index) > '9')
        {
            return false;
        }
    }

    const int hour =
        (value.at(0) - '0') * 10 + (value.at(1) - '0');
    const int minute =
        (value.at(3) - '0') * 10 + (value.at(4) - '0');
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
        || !isEventType(request.eventType)
        || request.eventType.size() > kCalendarEventSaveMaxEventTypeLength
        || !isTimeStatus(request.timeStatus)
        || request.timeStatus.size() > kCalendarEventSaveMaxTimeStatusLength)
    {
        return invalid(
            "Calendar event text fields must be non-blank and bounded."
            );
    }

    if (!isCanonicalDate(request.startDate)
        || !isCanonicalDate(request.endDate))
    {
        return invalid(
            "Calendar event dates must be valid canonical ISO dates."
            );
    }

    if (request.endDate < request.startDate)
    {
        return invalid(
            "Calendar event end date must not precede its start date."
            );
    }

    const std::string_view timeStatus = trimAscii(request.timeStatus);
    const bool hasStartTime = request.startTime.has_value();
    const bool hasEndTime = request.endTime.has_value();
    if (hasStartTime != hasEndTime)
    {
        return invalid(
            "Calendar event start and end times must be both absent or both present."
            );
    }

    if ((hasStartTime && !isCanonicalTime(*request.startTime))
        || (hasEndTime && !isCanonicalTime(*request.endTime)))
    {
        return invalid(
            "Calendar event times must be valid canonical 24-hour times."
            );
    }

    if (request.allDay)
    {
        if (timeStatus != "Timed" || hasStartTime || hasEndTime)
        {
            return invalid(
                "All-day calendar events require Timed status and no times."
                );
        }

        return Domain::Result<void>::success();
    }

    if (timeStatus == "Timed")
    {
        if (!hasStartTime || !hasEndTime)
        {
            return invalid(
                "Timed calendar events require both start and end times."
                );
        }

        if (request.startDate == request.endDate
            && *request.endTime <= *request.startTime)
        {
            return invalid(
                "Calendar event end time must be after its start time."
                );
        }
    }
    else if (hasStartTime || hasEndTime)
    {
        return invalid(
            "Unknown or unconfirmed calendar events require no times."
            );
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
