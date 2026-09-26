#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ClassMngr::Next::Application
{
struct CalendarEventCampusCode final
{
    std::string normalized;
    std::string caseFolded;
};

struct CalendarEventCampusVisibilityPolicy final
{
    // The feature boundary supplies trimmed, simply case-folded title and
    // campus-code strings. Campus codes retain their normalized form for
    // current-campus identity and a folded form for literal title matching.
    // Token boundaries use ASCII letters and digits after case folding.
    [[nodiscard]] static bool eventMatchesCampus(
        std::string_view normalizedTitle,
        const std::vector<CalendarEventCampusCode>& currentCampusCodes,
        const std::vector<CalendarEventCampusCode>& knownCampusCodes,
        bool showAllCampuses
        ) noexcept
    {
        if (showAllCampuses || normalizedTitle.empty())
        {
            return true;
        }

        if (currentCampusCodes.empty() || knownCampusCodes.empty())
        {
            return true;
        }

        bool matchedKnownCampus = false;
        for (const CalendarEventCampusCode& code : knownCampusCodes)
        {
            if (code.caseFolded.empty()
                || !containsCampusCode(normalizedTitle, code.caseFolded))
            {
                continue;
            }

            matchedKnownCampus = true;
            for (const CalendarEventCampusCode& currentCode :
                 currentCampusCodes)
            {
                if (currentCode.normalized == code.normalized)
                {
                    return true;
                }
            }
        }

        return !matchedKnownCampus;
    }

private:
    [[nodiscard]] static bool isAsciiAlphaNumeric(
        const char value
        ) noexcept
    {
        return (value >= 'A' && value <= 'Z')
            || (value >= 'a' && value <= 'z')
            || (value >= '0' && value <= '9');
    }

    [[nodiscard]] static bool containsCampusCode(
        const std::string_view title,
        const std::string_view code
        ) noexcept
    {
        if (code.empty())
        {
            return false;
        }

        std::size_t position = title.find(code);
        while (position != std::string_view::npos)
        {
            const std::size_t end = position + code.size();
            const bool hasLeftBoundary = position == 0
                || !isAsciiAlphaNumeric(title[position - 1]);
            const bool hasRightBoundary = end == title.size()
                || !isAsciiAlphaNumeric(title[end]);
            if (hasLeftBoundary && hasRightBoundary)
            {
                return true;
            }

            position = title.find(code, position + 1);
        }

        return false;
    }
};
}
