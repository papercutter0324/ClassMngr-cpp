#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ClassMngr::Next::Domain
{

enum class IsoWeekday
{
    Monday = 1,
    Tuesday = 2,
    Wednesday = 3,
    Thursday = 4,
    Friday = 5,
    Saturday = 6,
    Sunday = 7
};

// A valid date in the proleptic Gregorian calendar, represented independently
// of platform date libraries. Day numbers are zero-based with 0001-01-01 as 0.
class GregorianDate final
{
public:
    using DayNumber = std::int64_t;

    [[nodiscard]] static std::optional<GregorianDate> fromIsoString(
        const std::string_view value
        ) noexcept
    {
        if (value.size() != 10 || value[4] != '-' || value[7] != '-')
        {
            return std::nullopt;
        }

        for (std::size_t index = 0; index < value.size(); ++index)
        {
            if (index == 4 || index == 7)
            {
                continue;
            }
            if (value[index] < '0' || value[index] > '9')
            {
                return std::nullopt;
            }
        }

        const int year =
            (value[0] - '0') * 1'000
            + (value[1] - '0') * 100
            + (value[2] - '0') * 10
            + value[3] - '0';
        const int month =
            (value[5] - '0') * 10 + value[6] - '0';
        const int day =
            (value[8] - '0') * 10 + value[9] - '0';
        return fromComponents(year, month, day);
    }

    [[nodiscard]] static std::optional<GregorianDate> fromComponents(
        const int year,
        const int month,
        const int day
        ) noexcept
    {
        const int monthLength = daysInMonth(year, month);
        if (monthLength == 0 || day < 1 || day > monthLength)
        {
            return std::nullopt;
        }

        return GregorianDate(
            year,
            month,
            day,
            dayNumberForComponents(year, month, day)
            );
    }

    [[nodiscard]] static std::optional<GregorianDate> fromDayNumber(
        const DayNumber value
        ) noexcept
    {
        if (value < 0 || value > maximumDayNumber())
        {
            return std::nullopt;
        }

        int firstYear = 1;
        int afterLastYear = 10'000;
        while (firstYear + 1 < afterLastYear)
        {
            const int middleYear =
                firstYear + (afterLastYear - firstYear) / 2;
            if (daysBeforeYear(middleYear) <= value)
            {
                firstYear = middleYear;
            }
            else
            {
                afterLastYear = middleYear;
            }
        }

        int dayOfYear = static_cast<int>(value - daysBeforeYear(firstYear));
        int month = 1;
        while (month <= 12)
        {
            const int monthLength = daysInMonth(firstYear, month);
            if (dayOfYear < monthLength)
            {
                return GregorianDate(
                    firstYear,
                    month,
                    dayOfYear + 1,
                    value
                    );
            }
            dayOfYear -= monthLength;
            ++month;
        }

        return std::nullopt;
    }

    [[nodiscard]] static constexpr DayNumber maximumDayNumber() noexcept
    {
        return daysBeforeYear(10'000) - 1;
    }

    [[nodiscard]] static constexpr bool isLeapYear(
        const int year
        ) noexcept
    {
        return year >= 1 && year <= 9'999
            && (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0));
    }

    [[nodiscard]] static constexpr int daysInMonth(
        const int year,
        const int month
        ) noexcept
    {
        if (year < 1 || year > 9'999 || month < 1 || month > 12)
        {
            return 0;
        }

        constexpr int daysPerMonth[] = {
            31, 28, 31, 30, 31, 30,
            31, 31, 30, 31, 30, 31
        };
        if (month == 2 && isLeapYear(year))
        {
            return 29;
        }
        return daysPerMonth[month - 1];
    }

    [[nodiscard]] constexpr int year() const noexcept
    {
        return m_year;
    }

    [[nodiscard]] constexpr int month() const noexcept
    {
        return m_month;
    }

    [[nodiscard]] constexpr int day() const noexcept
    {
        return m_day;
    }

    [[nodiscard]] constexpr DayNumber dayNumber() const noexcept
    {
        return m_dayNumber;
    }

    [[nodiscard]] constexpr IsoWeekday weekday() const noexcept
    {
        return static_cast<IsoWeekday>(m_dayNumber % 7 + 1);
    }

    [[nodiscard]] std::string toIsoString() const
    {
        std::string result(10, '0');
        result[0] = static_cast<char>('0' + m_year / 1'000);
        result[1] = static_cast<char>('0' + m_year / 100 % 10);
        result[2] = static_cast<char>('0' + m_year / 10 % 10);
        result[3] = static_cast<char>('0' + m_year % 10);
        result[4] = '-';
        result[5] = static_cast<char>('0' + m_month / 10);
        result[6] = static_cast<char>('0' + m_month % 10);
        result[7] = '-';
        result[8] = static_cast<char>('0' + m_day / 10);
        result[9] = static_cast<char>('0' + m_day % 10);
        return result;
    }

    friend bool operator==(const GregorianDate&, const GregorianDate&) =
        default;

private:
    constexpr GregorianDate(
        const int year,
        const int month,
        const int day,
        const DayNumber dayNumber
        ) noexcept
        : m_year(year)
        , m_month(month)
        , m_day(day)
        , m_dayNumber(dayNumber)
    {
    }

    [[nodiscard]] static constexpr DayNumber daysBeforeYear(
        const int year
        ) noexcept
    {
        const DayNumber completedYears = year - 1;
        return completedYears * 365
            + completedYears / 4
            - completedYears / 100
            + completedYears / 400;
    }

    [[nodiscard]] static constexpr DayNumber dayNumberForComponents(
        const int year,
        const int month,
        const int day
        ) noexcept
    {
        DayNumber result = daysBeforeYear(year);
        for (int currentMonth = 1; currentMonth < month; ++currentMonth)
        {
            result += daysInMonth(year, currentMonth);
        }
        return result + day - 1;
    }

    int m_year;
    int m_month;
    int m_day;
    DayNumber m_dayNumber;
};

} // namespace ClassMngr::Next::Domain
