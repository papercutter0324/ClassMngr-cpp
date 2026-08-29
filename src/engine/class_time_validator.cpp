#include "classmngr/engine/class_time_validator.h"

#include "classmngr/engine/class_info_config.h"

#include <cctype>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace classmngr::engine::ClassTimeValidator
{
namespace
{
std::string trimAsciiWhitespace(std::string_view value)
{
    std::size_t first = 0;
    while (first < value.size()
           && std::isspace(static_cast<unsigned char>(value[first])) != 0)
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first
           && std::isspace(static_cast<unsigned char>(value[last - 1])) != 0)
    {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

char upperAscii(char value)
{
    return static_cast<char>(
        std::toupper(static_cast<unsigned char>(value))
        );
}

bool equalsAsciiInsensitive(
    std::string_view left,
    std::string_view right
    )
{
    if (left.size() != right.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (upperAscii(left[index]) != upperAscii(right[index]))
        {
            return false;
        }
    }
    return true;
}

std::optional<std::string> canonicalWeekday(std::string_view value)
{
    const std::string trimmed = trimAsciiWhitespace(value);
    for (const std::string& day : ClassInfoConfig::days())
    {
        if (equalsAsciiInsensitive(trimmed, day))
        {
            return day;
        }
    }
    return std::nullopt;
}

bool isDigit(char value)
{
    return value >= '0' && value <= '9';
}

int twoDigits(std::string_view value, std::size_t offset)
{
    return (value[offset] - '0') * 10 + value[offset + 1] - '0';
}

std::optional<int> parseStrict24Hour(std::string_view value)
{
    if (value.size() != 5
        || value[2] != ':'
        || !isDigit(value[0])
        || !isDigit(value[1])
        || !isDigit(value[3])
        || !isDigit(value[4]))
    {
        return std::nullopt;
    }

    const int hour = twoDigits(value, 0);
    const int minute = twoDigits(value, 3);
    if (hour > 23 || minute > 59)
    {
        return std::nullopt;
    }
    return hour * 60 + minute;
}

std::optional<int> parseTwelveHour(std::string_view value)
{
    const std::string normalized = trimAsciiWhitespace(value);
    const std::size_t colon = normalized.find(':');
    if (colon == std::string::npos
        || colon == 0
        || colon > 2
        || colon + 3 >= normalized.size()
        || normalized[colon + 3] != ' ')
    {
        return std::nullopt;
    }

    if (normalized.size() != colon + 6
        || !isDigit(normalized[colon + 1])
        || !isDigit(normalized[colon + 2]))
    {
        return std::nullopt;
    }

    for (std::size_t index = 0; index < colon; ++index)
    {
        if (!isDigit(normalized[index]))
        {
            return std::nullopt;
        }
    }

    const char periodFirst = upperAscii(normalized[colon + 4]);
    const char periodSecond = upperAscii(normalized[colon + 5]);
    if ((periodFirst != 'A' && periodFirst != 'P')
        || periodSecond != 'M')
    {
        return std::nullopt;
    }

    const int hour = colon == 1
        ? normalized[0] - '0'
        : twoDigits(normalized, 0);
    const int minute = twoDigits(normalized, colon + 1);
    if (hour < 1 || hour > 12 || minute > 59)
    {
        return std::nullopt;
    }

    // QTime's h:mm AP round-trip rejects a leading zero hour.  Enforce the
    // same rule here so validation and normalization agree across platforms.
    if (colon == 2 && normalized[0] == '0')
    {
        return std::nullopt;
    }

    int hour24 = hour % 12;
    if (periodFirst == 'P')
    {
        hour24 += 12;
    }
    return hour24 * 60 + minute;
}

std::optional<int> parseClassTime(std::string_view value)
{
    const std::string trimmed = trimAsciiWhitespace(value);
    if (const auto strict = parseStrict24Hour(trimmed))
    {
        return strict;
    }
    return parseTwelveHour(trimmed);
}

std::string canonicalTime(int minutes)
{
    const int hour24 = minutes / 60;
    const int minute = minutes % 60;
    const int hour12 = hour24 % 12 == 0 ? 12 : hour24 % 12;
    const char* period = hour24 < 12 ? "AM" : "PM";

    std::string result = std::to_string(hour12);
    result += ':';
    if (minute < 10)
    {
        result += '0';
    }
    result += std::to_string(minute);
    result += ' ';
    result += period;
    return result;
}

std::string canonical24HourTime(int minutes)
{
    const int hour = minutes / 60;
    const int minute = minutes % 60;
    std::string result = hour < 10
        ? "0" + std::to_string(hour)
        : std::to_string(hour);
    result += ':';
    if (minute < 10)
    {
        result += '0';
    }
    result += std::to_string(minute);
    return result;
}

std::string fieldName(
    std::string_view prefix,
    std::size_t row,
    std::string_view field
    )
{
    return std::string(prefix)
        + '[' + std::to_string(row) + "]." + std::string(field);
}

void addIssue(
    ValidationResult& result,
    std::string_view code,
    std::string field
    )
{
    result.add(ValidationIssue{
        std::string(code),
        std::move(field),
        ValidationSeverity::Error
    });
}
} // namespace

ClassTime normalized(const ClassTime& time)
{
    ClassTime result = time;
    result.day = canonicalWeekday(time.day).value_or(
        trimAsciiWhitespace(time.day)
        );

    if (const auto start = parseClassTime(time.startTime))
    {
        result.startTime = canonicalTime(*start);
    }
    else
    {
        result.startTime = trimAsciiWhitespace(time.startTime);
    }

    if (const auto end = parseClassTime(time.endTime))
    {
        result.endTime = canonicalTime(*end);
    }
    else
    {
        result.endTime = trimAsciiWhitespace(time.endTime);
    }

    return result;
}

ValidationResult validate(
    const std::vector<ClassTime>& times,
    std::string_view fieldPrefix
    )
{
    ValidationResult result;
    std::map<std::string, std::vector<std::size_t>> rowsBySlot;

    for (std::size_t row = 0; row < times.size(); ++row)
    {
        const ClassTime& time = times[row];
        const std::string dayField = fieldName(fieldPrefix, row, "day");
        const std::string startField = fieldName(
            fieldPrefix,
            row,
            "startTime"
            );
        const std::string endField = fieldName(fieldPrefix, row, "endTime");

        const auto weekday = canonicalWeekday(time.day);
        if (!weekday)
        {
            addIssue(result, "schedule.weekday.invalid", dayField);
        }

        const auto start = parseClassTime(time.startTime);
        if (!start)
        {
            addIssue(result, "schedule.time.invalid_format", startField);
        }

        const auto end = parseClassTime(time.endTime);
        if (!end)
        {
            addIssue(result, "schedule.time.invalid_format", endField);
        }

        if (start && end && *end <= *start)
        {
            addIssue(
                result,
                "schedule.time.end_not_after_start",
                endField
                );
        }

        if (weekday && start && end)
        {
            const std::string slot = *weekday
                + '|'
                + canonical24HourTime(*start)
                + '|'
                + canonical24HourTime(*end);
            rowsBySlot[slot].push_back(row);
        }
    }

    for (const auto& [slot, rows] : rowsBySlot)
    {
        (void)slot;
        if (rows.size() < 2)
        {
            continue;
        }
        for (const std::size_t row : rows)
        {
            addIssue(
                result,
                "class_time.duplicate_slot",
                fieldName(fieldPrefix, row, "startTime")
                );
        }
    }

    return result;
}

} // namespace classmngr::engine::ClassTimeValidator
