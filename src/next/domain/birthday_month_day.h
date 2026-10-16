#pragma once

#include "next/domain/gregorian_date.h"

#include <optional>

namespace ClassMngr::Next::Domain
{

// A month and day that occur in the Gregorian calendar, including leap day.
// The value is created only when the components are valid in leap year 2000.
class BirthdayMonthDay final
{
public:
    [[nodiscard]] static std::optional<BirthdayMonthDay> fromComponents(
        const int month,
        const int day
        ) noexcept
    {
        if (!GregorianDate::fromComponents(2000, month, day))
        {
            return std::nullopt;
        }

        return BirthdayMonthDay(month, day);
    }

    [[nodiscard]] constexpr int month() const noexcept
    {
        return m_month;
    }

    [[nodiscard]] constexpr int day() const noexcept
    {
        return m_day;
    }

    friend constexpr bool operator==(
        const BirthdayMonthDay&,
        const BirthdayMonthDay&
        ) noexcept = default;

private:
    constexpr BirthdayMonthDay(const int month, const int day) noexcept
        : m_month(month)
        , m_day(day)
    {
    }

    const int m_month;
    const int m_day;
};

} // namespace ClassMngr::Next::Domain
