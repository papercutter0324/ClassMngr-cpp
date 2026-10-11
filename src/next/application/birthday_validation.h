#pragma once

#include "next/application/qt_compatible_text.h"

#include <string>
#include <string_view>

namespace ClassMngr::Next::Application
{

[[nodiscard]] inline bool isBirthdayTextBlank(
    const std::u16string_view birthday
    )
{
    return trimQtWhitespace(birthday).empty();
}

[[nodiscard]] inline bool isValidMonthDayBirthday(
    const std::u16string_view birthday
    )
{
    const std::u16string normalized = trimQtWhitespace(birthday);
    if (normalized.size() != 5 || normalized[2] != u'-')
    {
        return false;
    }

    const auto parseTwoDigits = [&normalized](const std::size_t offset)
        -> int
    {
        const char16_t tens = normalized[offset];
        const char16_t ones = normalized[offset + 1];
        if (tens < u'0' || tens > u'9' || ones < u'0' || ones > u'9')
        {
            return -1;
        }
        return (tens - u'0') * 10 + (ones - u'0');
    };

    const int month = parseTwoDigits(0);
    const int day = parseTwoDigits(3);
    if (month < 1 || month > 12 || day < 1)
    {
        return false;
    }

    // The former UI check parsed the month and day in leap year 2000.
    constexpr int daysInMonth[]{
        31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    return day <= daysInMonth[month - 1];
}

} // namespace ClassMngr::Next::Application
