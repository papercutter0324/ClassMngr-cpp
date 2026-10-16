#pragma once

#include "next/application/qt_compatible_text.h"
#include "next/domain/birthday_month_day.h"

#include <optional>
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

[[nodiscard]] inline std::optional<Domain::BirthdayMonthDay>
parseMonthDayBirthday(
    const std::u16string_view birthday
    )
{
    const std::u16string normalized = trimQtWhitespace(birthday);
    if (normalized.size() != 5 || normalized[2] != u'-')
    {
        return std::nullopt;
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
    // The former UI check parsed the month and day in leap year 2000.
    return Domain::BirthdayMonthDay::fromComponents(month, day);
}

[[nodiscard]] inline bool isValidMonthDayBirthday(
    const std::u16string_view birthday
    )
{
    return parseMonthDayBirthday(birthday).has_value();
}

} // namespace ClassMngr::Next::Application
