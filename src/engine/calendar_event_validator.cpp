#include "classmngr/engine/calendar_event_validator.h"

#include "classmngr/engine/calendar_event_rules.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace classmngr::engine
{
namespace
{
using std::chrono::days;
using std::chrono::month;
using std::chrono::sys_days;
using std::chrono::year;

constexpr std::size_t MinutesPerDay = 24U * 60U;
constexpr std::size_t LastMinuteOfDay = MinutesPerDay - 1U;

bool isAsciiWhitespace(char value) noexcept
{
    switch (value)
    {
    case ' ':
    case '\t':
    case '\n':
    case '\v':
    case '\f':
    case '\r':
        return true;
    default:
        return false;
    }
}

std::string trimAsciiWhitespace(std::string_view value)
{
    std::size_t first = 0;
    while (first < value.size() && isAsciiWhitespace(value[first]))
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first && isAsciiWhitespace(value[last - 1]))
    {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

std::string simplifyAsciiWhitespace(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    bool pendingSpace = false;

    for (const char character : value)
    {
        if (isAsciiWhitespace(character))
        {
            if (!result.empty())
            {
                pendingSpace = true;
            }
            continue;
        }

        if (pendingSpace)
        {
            result.push_back(' ');
            pendingSpace = false;
        }
        result.push_back(character);
    }

    return result;
}

bool decodeUtf8(
    std::string_view value,
    std::size_t offset,
    std::size_t* length
    ) noexcept
{
    if (length == nullptr || offset >= value.size())
    {
        return false;
    }

    const unsigned char first = static_cast<unsigned char>(value[offset]);
    std::size_t sequenceLength = 0;
    unsigned codePoint = 0;
    unsigned minimum = 0;

    if (first <= 0x7fU)
    {
        *length = 1;
        return true;
    }
    if (first >= 0xc2U && first <= 0xdfU)
    {
        sequenceLength = 2;
        codePoint = first & 0x1fU;
        minimum = 0x80U;
    }
    else if (first >= 0xe0U && first <= 0xefU)
    {
        sequenceLength = 3;
        codePoint = first & 0x0fU;
        minimum = 0x800U;
    }
    else if (first >= 0xf0U && first <= 0xf4U)
    {
        sequenceLength = 4;
        codePoint = first & 0x07U;
        minimum = 0x10000U;
    }
    else
    {
        return false;
    }

    if (offset + sequenceLength > value.size())
    {
        return false;
    }

    for (std::size_t index = 1; index < sequenceLength; ++index)
    {
        const unsigned char continuation = static_cast<unsigned char>(
            value[offset + index]
            );
        if ((continuation & 0xc0U) != 0x80U)
        {
            return false;
        }
        codePoint = (codePoint << 6U) | (continuation & 0x3fU);
    }

    if (codePoint < minimum || codePoint > 0x10ffffU
        || (codePoint >= 0xd800U && codePoint <= 0xdfffU))
    {
        return false;
    }

    *length = sequenceLength;
    return true;
}

std::size_t utf8Length(std::string_view value) noexcept
{
    std::size_t length = 0;
    for (std::size_t offset = 0; offset < value.size(); ++length)
    {
        std::size_t sequenceLength = 0;
        if (decodeUtf8(value, offset, &sequenceLength))
        {
            offset += sequenceLength;
        }
        else
        {
            ++offset;
        }
    }

    return length;
}

ValidationIssue issue(
    std::string code,
    std::string field
    )
{
    return {
        .code = std::move(code),
        .field = std::move(field),
        .severity = ValidationSeverity::Error
    };
}

void addIssue(
    ValidationResult& result,
    std::string_view code,
    std::string_view field
    )
{
    result.add(issue(std::string(code), std::string(field)));
}

bool isSupportedEventType(std::string_view value)
{
    // The rules object owns the exact supported values.  Comparing its
    // canonical result with the original retains invalid input for validate().
    return CalendarEventRules::normalizedEventType(value) == value;
}

bool isSupportedTimeStatus(std::string_view value)
{
    return CalendarEventRules::normalizedTimeStatus(value) == value;
}

std::string normalizedEventType(std::string_view value)
{
    const std::string trimmed = trimAsciiWhitespace(value);
    const std::string canonical = CalendarEventRules::normalizedEventType(
        trimmed
        );
    return canonical == trimmed ? canonical : trimmed;
}

std::string normalizedTimeStatus(std::string_view value)
{
    const std::string trimmed = trimAsciiWhitespace(value);
    const std::string canonical = CalendarEventRules::normalizedTimeStatus(
        trimmed
        );
    return canonical == trimmed ? canonical : trimmed;
}

bool isSupportedFrequency(CalendarEventRepeatFrequency frequency) noexcept
{
    switch (frequency)
    {
    case CalendarEventRepeatFrequency::Daily:
    case CalendarEventRepeatFrequency::Weekly:
    case CalendarEventRepeatFrequency::Monthly:
        return true;
    }

    return false;
}

bool isValidTime(const std::optional<std::chrono::minutes>& value) noexcept
{
    if (!value.has_value())
    {
        return false;
    }

    const auto count = value->count();
    return count >= 0
        && static_cast<unsigned long long>(count) <= LastMinuteOfDay;
}

bool dateBefore(const CalendarDate& left, const CalendarDate& right)
{
    return sys_days{left} < sys_days{right};
}

bool dateEqual(const CalendarDate& left, const CalendarDate& right)
{
    return sys_days{left} == sys_days{right};
}

std::optional<CalendarDate> nextRepeatDate(
    const CalendarDate& date,
    CalendarEventRepeatFrequency frequency
    )
{
    if (!date.ok())
    {
        return std::nullopt;
    }

    switch (frequency)
    {
    case CalendarEventRepeatFrequency::Daily:
    case CalendarEventRepeatFrequency::Weekly:
    {
        const days increment = frequency == CalendarEventRepeatFrequency::Daily
            ? days{1}
            : days{7};
        const CalendarDate next{sys_days{date} + increment};
        return next.ok()
            ? std::optional<CalendarDate>{next}
            : std::nullopt;
    }

    case CalendarEventRepeatFrequency::Monthly:
    {
        const int currentYear = static_cast<int>(date.year());
        const unsigned currentMonth = static_cast<unsigned>(date.month());
        const unsigned currentDay = static_cast<unsigned>(date.day());
        const bool wrapsYear = currentMonth == 12U;
        const int nextYearValue = currentYear + (wrapsYear ? 1 : 0);
        const year nextYear{nextYearValue};
        if (!nextYear.ok())
        {
            return std::nullopt;
        }

        const unsigned nextMonthValue = wrapsYear
            ? 1U
            : currentMonth + 1U;
        const month nextMonth{nextMonthValue};
        const auto lastDay = std::chrono::year_month_day_last{
            nextYear,
            std::chrono::month_day_last{nextMonth}
        }.day();
        const unsigned nextDay = std::min(
            currentDay,
            static_cast<unsigned>(lastDay)
            );
        const CalendarDate next{nextYear, nextMonth, std::chrono::day{nextDay}};
        return next.ok()
            ? std::optional<CalendarDate>{next}
            : std::nullopt;
    }
    }

    return std::nullopt;
}

void addIndexedIssues(
    ValidationResult& destination,
    const ValidationResult& source,
    std::size_t index
    )
{
    for (ValidationIssue memberIssue : source.issues())
    {
        const std::string prefix = "events[" + std::to_string(index) + "]";
        memberIssue.field = memberIssue.field.empty()
            ? prefix
            : prefix + "." + memberIssue.field;
        memberIssue.row = static_cast<int>(index);
        destination.add(std::move(memberIssue));
    }
}
} // namespace

CalendarEvent CalendarEventValidator::normalized(const CalendarEvent& event)
{
    CalendarEvent result = event;
    result.title = simplifyAsciiWhitespace(event.title);
    result.eventType = normalizedEventType(event.eventType);
    result.timeStatus = normalizedTimeStatus(event.timeStatus);
    result.repeatSeriesId = trimAsciiWhitespace(event.repeatSeriesId);
    return result;
}

ValidationResult CalendarEventValidator::validate(const CalendarEvent& event)
{
    ValidationResult result;
    const std::string title = simplifyAsciiWhitespace(event.title);

    if (title.empty())
    {
        addIssue(result, "calendar.title.required", "title");
    }
    else if (utf8Length(title) > MaximumTitleLength)
    {
        addIssue(result, "validation.length.out_of_bounds", "title");
    }

    if (!isSupportedEventType(event.eventType))
    {
        addIssue(result, "validation.enum.invalid_value", "eventType");
    }
    if (!isSupportedTimeStatus(event.timeStatus))
    {
        addIssue(result, "validation.enum.invalid_value", "timeStatus");
    }
    if (utf8Length(event.repeatSeriesId) > MaximumRepeatSeriesIdLength)
    {
        addIssue(
            result,
            "validation.length.out_of_bounds",
            "repeatSeriesId"
            );
    }

    if (!event.startDate.ok())
    {
        addIssue(result, "calendar.date.invalid", "startDate");
    }
    if (!event.endDate.ok())
    {
        addIssue(result, "calendar.date.invalid", "endDate");
    }
    if (event.startDate.ok() && event.endDate.ok()
        && dateBefore(event.endDate, event.startDate))
    {
        addIssue(result, "calendar.date.end_before_start", "endDate");
    }

    if (!isSupportedTimeStatus(event.timeStatus))
    {
        return result;
    }

    if (event.allDay)
    {
        if (event.timeStatus != "Timed")
        {
            addIssue(
                result,
                "calendar.time_status.all_day_requires_timed",
                "timeStatus"
                );
        }
        return result;
    }

    if (event.timeStatus == "Timed")
    {
        if (!isValidTime(event.startTime))
        {
            addIssue(result, "calendar.time.invalid", "startTime");
        }
        if (!isValidTime(event.endTime))
        {
            addIssue(result, "calendar.time.invalid", "endTime");
        }

        if (event.startDate.ok() && event.endDate.ok()
            && isValidTime(event.startTime) && isValidTime(event.endTime)
            && (dateBefore(event.endDate, event.startDate)
                || (dateEqual(event.endDate, event.startDate)
                    && *event.endTime <= *event.startTime)))
        {
            addIssue(
                result,
                "calendar.time.end_not_after_start",
                "endTime"
                );
        }
        return result;
    }

    if (event.startTime.has_value() || event.endTime.has_value())
    {
        addIssue(
            result,
            "calendar.time_status.requires_empty_times",
            "timeStatus"
            );
    }

    return result;
}

ValidationResult CalendarEventValidator::validateRecurrence(
    const CalendarEvent& event,
    CalendarEventRepeatFrequency frequency,
    const CalendarDate& untilDate
    )
{
    ValidationResult result = validate(event);

    if (!isSupportedFrequency(frequency))
    {
        addIssue(result, "validation.enum.invalid_value", "repeat.frequency");
    }

    if (!untilDate.ok())
    {
        addIssue(
            result,
            "calendar.repeat.until_date.invalid",
            "repeat.untilDate"
            );
        return result;
    }

    if (!event.startDate.ok())
    {
        return result;
    }

    if (dateBefore(untilDate, event.startDate))
    {
        addIssue(
            result,
            "calendar.repeat.until_before_start",
            "repeat.untilDate"
            );
        return result;
    }

    if (!isSupportedFrequency(frequency))
    {
        return result;
    }

    if (estimatedRepeatOccurrences(
            event.startDate,
            untilDate,
            frequency
            ) > MaximumRepeatOccurrences)
    {
        addIssue(
            result,
            "calendar.repeat.too_many_occurrences",
            "repeat.untilDate"
            );
    }

    return result;
}

ValidationResult CalendarEventValidator::validateSeries(
    const std::vector<CalendarEvent>& events
    )
{
    ValidationResult result;
    std::map<std::string, std::vector<std::size_t>> rowsByRepeatSeries;

    for (std::size_t index = 0; index < events.size(); ++index)
    {
        const CalendarEvent& event = events[index];
        addIndexedIssues(result, validate(event), index);

        if (!event.repeatSeriesId.empty())
        {
            rowsByRepeatSeries[event.repeatSeriesId].push_back(index);
        }
    }

    for (const auto& entry : rowsByRepeatSeries)
    {
        const std::vector<std::size_t>& rows = entry.second;
        if (rows.size() <= MaximumRepeatOccurrences)
        {
            continue;
        }

        for (const std::size_t row : rows)
        {
            ValidationIssue capIssue = issue(
                "calendar.repeat.too_many_occurrences",
                "events[" + std::to_string(row) + "].repeatSeriesId"
                );
            capIssue.row = static_cast<int>(row);
            result.add(std::move(capIssue));
        }
    }

    return result;
}

int CalendarEventValidator::estimatedRepeatOccurrences(
    const CalendarDate& startDate,
    const CalendarDate& endDate,
    CalendarEventRepeatFrequency frequency
    )
{
    if (!startDate.ok() || !endDate.ok() || dateBefore(endDate, startDate))
    {
        return 0;
    }

    int occurrences = 0;
    CalendarDate occurrence = startDate;
    while (true)
    {
        ++occurrences;
        if (occurrences > CalendarEventValidator::MaximumRepeatOccurrences)
        {
            return occurrences;
        }

        const std::optional<CalendarDate> next = nextRepeatDate(
            occurrence,
            frequency
            );
        if (!next.has_value() || dateBefore(endDate, *next))
        {
            return occurrences;
        }

        occurrence = *next;
    }
}

} // namespace classmngr::engine
