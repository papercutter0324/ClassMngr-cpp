#include "classmngr/engine/calendar_event_rules.h"

#include <array>
#include <cctype>

namespace classmngr::engine
{
namespace
{
constexpr std::array<std::string_view, 6> EventTypes{
    "Vacation",
    "Holiday",
    "Workshop",
    "CM",
    "Meeting",
    "Other"
};

constexpr std::array<std::string_view, 3> TimeStatuses{
    "Timed",
    "Unknown",
    "Unconfirmed"
};

char upperAscii(char value)
{
    return static_cast<char>(
        std::toupper(static_cast<unsigned char>(value))
        );
}

std::string trimAsciiWhitespace(std::string_view value)
{
    std::size_t first = 0;
    while (
        first < value.size()
        && std::isspace(static_cast<unsigned char>(value[first])) != 0
        )
    {
        ++first;
    }

    std::size_t last = value.size();
    while (
        last > first
        && std::isspace(static_cast<unsigned char>(value[last - 1])) != 0
        )
    {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

std::string simplifiedLowerAscii(std::string_view value)
{
    std::string result;
    bool pendingSpace = false;

    for (const char character : value)
    {
        if (std::isspace(static_cast<unsigned char>(character)) != 0)
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

        result.push_back(
            static_cast<char>(
                std::tolower(static_cast<unsigned char>(character))
                )
            );
    }

    return result;
}

bool containsExact(
    const std::array<std::string_view, 6>& values,
    std::string_view candidate
    )
{
    for (const std::string_view value : values)
    {
        if (value == candidate)
        {
            return true;
        }
    }
    return false;
}

bool containsExact(
    const std::array<std::string_view, 3>& values,
    std::string_view candidate
    )
{
    for (const std::string_view value : values)
    {
        if (value == candidate)
        {
            return true;
        }
    }
    return false;
}

std::string normalizedCode(std::string_view code)
{
    std::string normalized = trimAsciiWhitespace(code);
    for (char& character : normalized)
    {
        character = upperAscii(character);
    }
    return normalized;
}

bool asciiAlphaNumeric(char value)
{
    return (
        value >= 'A' && value <= 'Z'
        ) || (
            value >= 'a' && value <= 'z'
            ) || (
                value >= '0' && value <= '9'
                );
}

bool matchesAt(
    std::string_view title,
    std::size_t offset,
    std::string_view code
    )
{
    if (offset + code.size() > title.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < code.size(); ++index)
    {
        if (upperAscii(title[offset + index]) != code[index])
        {
            return false;
        }
    }

    const bool leftBoundary =
        offset == 0 || !asciiAlphaNumeric(title[offset - 1]);
    const std::size_t end = offset + code.size();
    const bool rightBoundary =
        end == title.size() || !asciiAlphaNumeric(title[end]);
    return leftBoundary && rightBoundary;
}

bool containsCode(
    std::string_view title,
    std::string_view code
    )
{
    const std::string normalized = normalizedCode(code);
    if (normalized.empty())
    {
        return false;
    }

    for (std::size_t offset = 0; offset < title.size(); ++offset)
    {
        if (matchesAt(title, offset, normalized))
        {
            return true;
        }
    }

    return false;
}

bool containsNormalizedCode(
    const std::vector<std::string>& codes,
    std::string_view candidate
    )
{
    const std::string normalizedCandidate = normalizedCode(candidate);
    if (normalizedCandidate.empty())
    {
        return false;
    }

    for (const std::string& code : codes)
    {
        if (normalizedCode(code) == normalizedCandidate)
        {
            return true;
        }
    }

    return false;
}

bool hasNormalizedCode(const std::vector<std::string>& codes)
{
    for (const std::string& code : codes)
    {
        if (!normalizedCode(code).empty())
        {
            return true;
        }
    }
    return false;
}
} // namespace

std::string CalendarEventRules::normalizedEventType(
    std::string_view eventType
    )
{
    const std::string trimmed = trimAsciiWhitespace(eventType);
    return containsExact(EventTypes, trimmed)
        ? trimmed
        : "Other";
}

std::string CalendarEventRules::normalizedTimeStatus(
    std::string_view timeStatus
    )
{
    const std::string trimmed = trimAsciiWhitespace(timeStatus);
    return containsExact(TimeStatuses, trimmed)
        ? trimmed
        : "Timed";
}

bool CalendarEventRules::isStartOfTerm(
    std::string_view title,
    std::string_view eventType
    )
{
    if (normalizedEventType(eventType) != "Other")
    {
        return false;
    }

    const std::string normalizedTitle = simplifiedLowerAscii(title);
    return normalizedTitle == "new semester"
        || normalizedTitle == "start of term"
        || normalizedTitle == "term start"
        || normalizedTitle == "term starts";
}

bool CalendarEventRules::eventMatchesCampus(
    std::string_view title,
    const std::vector<std::string>& currentCampusCodes,
    const std::vector<std::string>& allCampusCodes,
    bool showAllCampuses
    )
{
    if (showAllCampuses)
    {
        return true;
    }

    if (trimAsciiWhitespace(title).empty())
    {
        return true;
    }

    if (
        !hasNormalizedCode(allCampusCodes)
        || !hasNormalizedCode(currentCampusCodes)
        )
    {
        return true;
    }

    bool containsAnyKnownCampus = false;
    for (const std::string& code : allCampusCodes)
    {
        if (!containsCode(title, code))
        {
            continue;
        }

        containsAnyKnownCampus = true;
        if (containsNormalizedCode(currentCampusCodes, code))
        {
            return true;
        }
    }

    return !containsAnyKnownCampus;
}

} // namespace classmngr::engine
