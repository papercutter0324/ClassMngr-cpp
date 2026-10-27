#pragma once

#include <array>
#include <string_view>
#include <vector>

namespace ClassMngr::Domain
{

// Canonical values used by both legacy and Next class schedule editors.
// Display localization belongs to each UI; item data stays in English.
class ClassScheduleCatalog final
{
public:
    inline static constexpr std::array<std::string_view, 7> weekdays{{
        "Monday", "Tuesday", "Wednesday", "Thursday",
        "Friday", "Saturday", "Sunday"
    }};
    inline static constexpr std::array<std::string_view, 7> regularHours{{
        "3", "4", "5", "6", "7", "8", "9"
    }};
    inline static constexpr std::array<std::string_view, 12> intensiveHours{{
        "1", "2", "3", "4", "5", "6",
        "7", "8", "9", "10", "11", "12"
    }};
    inline static constexpr std::array<std::string_view, 5> startMinutes{{
        ":00", ":05", ":25", ":30", ":35"
    }};
    inline static constexpr std::array<std::string_view, 2> periods{{
        "PM", "AM"
    }};
    inline static constexpr std::array<int, 5> endTimeDurations{{
        55, 85, 115, 175, 235
    }};
    inline static constexpr int latestEndTimeMinutes = 21 * 60 + 55;

    [[nodiscard]] static std::vector<std::string_view> days()
    {
        return {weekdays.begin(), weekdays.end()};
    }
    [[nodiscard]] static std::vector<std::string_view> regularStartHours()
    {
        return {regularHours.begin(), regularHours.end()};
    }
    [[nodiscard]] static std::vector<std::string_view> intensiveStartHours()
    {
        return {intensiveHours.begin(), intensiveHours.end()};
    }
    [[nodiscard]] static std::vector<std::string_view> startMinuteChoices()
    {
        return {startMinutes.begin(), startMinutes.end()};
    }
    [[nodiscard]] static std::vector<std::string_view> periodChoices()
    {
        return {periods.begin(), periods.end()};
    }
};

}  // namespace ClassMngr::Domain
