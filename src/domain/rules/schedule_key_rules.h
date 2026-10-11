#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace ScheduleKeyRules
{

enum class Weekday
{
    Monday,
    Tuesday,
    Wednesday,
    Thursday,
    Friday,
    Saturday,
    Sunday
};

struct ParsedWeekday final
{
    Weekday value = Weekday::Monday;
    std::u16string_view canonicalName;
};

struct ParsedTime final
{
    unsigned hour = 0;
    unsigned minute = 0;
};

template <typename CaseInsensitiveEquals>
[[nodiscard]] inline std::optional<ParsedWeekday> parseWeekday(
    const std::u16string_view input,
    CaseInsensitiveEquals&& equals
    )
{
    struct WeekdayEntry final
    {
        Weekday value;
        std::u16string_view name;
    };

    constexpr std::array Weekdays{
        WeekdayEntry{Weekday::Monday, u"Monday"},
        WeekdayEntry{Weekday::Tuesday, u"Tuesday"},
        WeekdayEntry{Weekday::Wednesday, u"Wednesday"},
        WeekdayEntry{Weekday::Thursday, u"Thursday"},
        WeekdayEntry{Weekday::Friday, u"Friday"},
        WeekdayEntry{Weekday::Saturday, u"Saturday"},
        WeekdayEntry{Weekday::Sunday, u"Sunday"}
    };

    for (const WeekdayEntry& weekday : Weekdays)
    {
        if (equals(input, weekday.name))
        {
            return ParsedWeekday{weekday.value, weekday.name};
        }
    }

    return std::nullopt;
}

[[nodiscard]] inline std::optional<ParsedTime> parseTime(
    const std::u16string_view input
    ) noexcept
{
    if (input.size() != 5
        || input[2] != u':'
        || input[0] < u'0'
        || input[0] > u'9'
        || input[1] < u'0'
        || input[1] > u'9'
        || input[3] < u'0'
        || input[3] > u'9'
        || input[4] < u'0'
        || input[4] > u'9')
    {
        return std::nullopt;
    }

    const unsigned hour =
        static_cast<unsigned>(input[0] - u'0') * 10
        + static_cast<unsigned>(input[1] - u'0');
    const unsigned minute =
        static_cast<unsigned>(input[3] - u'0') * 10
        + static_cast<unsigned>(input[4] - u'0');
    if (hour >= 24 || minute >= 60)
    {
        return std::nullopt;
    }

    return ParsedTime{hour, minute};
}

} // namespace ScheduleKeyRules
