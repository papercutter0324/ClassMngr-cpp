#pragma once

#include "next/application/calendar_event_series_create_port.h"
#include "next/domain/calendar_event_timing.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace ClassMngr::Next::Application
{

enum class CalendarEventRepeatFrequency
{
    Daily,
    Weekly,
    Monthly
};

namespace CalendarEventRepeatOccurrencePlanDetail
{

struct GregorianDate final
{
    int year;
    int month;
    int day;
};

[[nodiscard]] inline bool isLeapYear(const int year) noexcept
{
    return year % 400 == 0 || (year % 4 == 0 && year % 100 != 0);
}

[[nodiscard]] inline int daysInMonth(
    const int year,
    const int month
    ) noexcept
{
    constexpr int DaysPerMonth[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };
    if (month == 2 && isLeapYear(year))
    {
        return 29;
    }
    return DaysPerMonth[month - 1];
}

[[nodiscard]] inline std::optional<GregorianDate> parseDate(
    const std::string_view value
    ) noexcept
{
    if (!Domain::CalendarEventTiming::isCanonicalDate(value))
    {
        return std::nullopt;
    }

    const auto twoDigits = [&value](const std::size_t offset) {
        return (value[offset] - '0') * 10 + value[offset + 1] - '0';
    };
    const int year =
        (value[0] - '0') * 1'000
        + (value[1] - '0') * 100
        + (value[2] - '0') * 10
        + value[3] - '0';
    return GregorianDate{year, twoDigits(5), twoDigits(8)};
}

[[nodiscard]] inline int daysBeforeYear(const int year) noexcept
{
    const int precedingYear = year - 1;
    return precedingYear * 365
        + precedingYear / 4
        - precedingYear / 100
        + precedingYear / 400;
}

[[nodiscard]] inline int toOrdinal(const GregorianDate date) noexcept
{
    int result = daysBeforeYear(date.year);
    for (int month = 1; month < date.month; ++month)
    {
        result += daysInMonth(date.year, month);
    }
    return result + date.day - 1;
}

[[nodiscard]] inline std::optional<GregorianDate> fromOrdinal(
    const int ordinal
    ) noexcept
{
    if (ordinal < 0 || ordinal >= daysBeforeYear(10'000))
    {
        return std::nullopt;
    }

    int firstYear = 1;
    int afterLastYear = 10'000;
    while (firstYear + 1 < afterLastYear)
    {
        const int middleYear = firstYear + (afterLastYear - firstYear) / 2;
        if (daysBeforeYear(middleYear) <= ordinal)
        {
            firstYear = middleYear;
        }
        else
        {
            afterLastYear = middleYear;
        }
    }

    int dayOfYear = ordinal - daysBeforeYear(firstYear);
    for (int month = 1; month <= 12; ++month)
    {
        const int monthLength = daysInMonth(firstYear, month);
        if (dayOfYear < monthLength)
        {
            return GregorianDate{firstYear, month, dayOfYear + 1};
        }
        dayOfYear -= monthLength;
    }

    return std::nullopt;
}

[[nodiscard]] inline std::string formatDate(
    const GregorianDate date
    )
{
    std::string result(10, '0');
    result[0] = static_cast<char>('0' + date.year / 1'000);
    result[1] = static_cast<char>('0' + date.year / 100 % 10);
    result[2] = static_cast<char>('0' + date.year / 10 % 10);
    result[3] = static_cast<char>('0' + date.year % 10);
    result[4] = '-';
    result[5] = static_cast<char>('0' + date.month / 10);
    result[6] = static_cast<char>('0' + date.month % 10);
    result[7] = '-';
    result[8] = static_cast<char>('0' + date.day / 10);
    result[9] = static_cast<char>('0' + date.day % 10);
    return result;
}

[[nodiscard]] inline bool isSupportedFrequency(
    const CalendarEventRepeatFrequency frequency
    ) noexcept
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

[[nodiscard]] inline std::optional<GregorianDate> nextOccurrenceDate(
    const GregorianDate current,
    const CalendarEventRepeatFrequency frequency
    ) noexcept
{
    switch (frequency)
    {
    case CalendarEventRepeatFrequency::Daily:
        return fromOrdinal(toOrdinal(current) + 1);

    case CalendarEventRepeatFrequency::Weekly:
        return fromOrdinal(toOrdinal(current) + 7);

    case CalendarEventRepeatFrequency::Monthly:
    {
        int year = current.year;
        int month = current.month + 1;
        if (month > 12)
        {
            month = 1;
            ++year;
        }
        if (year > 9'999)
        {
            return std::nullopt;
        }
        return GregorianDate{
            year,
            month,
            std::min(current.day, daysInMonth(year, month))
        };
    }
    }

    return std::nullopt;
}

[[nodiscard]] inline Domain::Result<CalendarEventSeriesCreateRequest> invalid(
    std::string message
    )
{
    return Domain::Result<CalendarEventSeriesCreateRequest>::failure({
        .code = Domain::ErrorCode::InvalidInput,
        .message = std::move(message),
        .recoverable = false
    });
}

} // namespace CalendarEventRepeatOccurrencePlanDetail

[[nodiscard]] inline Domain::Result<CalendarEventSeriesCreateRequest>
planCalendarEventRepeatOccurrences(
    const CalendarEventSaveRequest& seed,
    std::string repeatSeriesId,
    const CalendarEventRepeatFrequency frequency,
    const std::string_view untilDate
    )
{
    using namespace CalendarEventRepeatOccurrencePlanDetail;

    if (!isSupportedFrequency(frequency))
    {
        return invalid("Calendar repeat frequency is invalid.");
    }

    const Domain::Result<void> seedValidation = seed.validate();
    if (!seedValidation)
    {
        return invalid(seedValidation.error().message);
    }

    const std::optional<GregorianDate> startDate = parseDate(seed.startDate);
    const std::optional<GregorianDate> seedEndDate = parseDate(seed.endDate);
    const std::optional<GregorianDate> finalStartDate = parseDate(untilDate);
    if (!startDate || !seedEndDate || !finalStartDate)
    {
        return invalid(
            "Calendar repeat seed and until dates must be canonical ISO dates."
            );
    }

    const int firstOrdinal = toOrdinal(*startDate);
    const int lastOrdinal = toOrdinal(*finalStartDate);
    if (lastOrdinal < firstOrdinal)
    {
        return invalid(
            "Calendar repeat until date must not precede the seed start date."
            );
    }

    const int durationDays = toOrdinal(*seedEndDate) - firstOrdinal;
    CalendarEventSeriesCreateRequest request;
    request.repeatSeriesId = std::move(repeatSeriesId);

    GregorianDate occurrenceStart = *startDate;
    while (toOrdinal(occurrenceStart) <= lastOrdinal)
    {
        if (request.occurrences.size()
            >= kCalendarEventSeriesCreateMaxOccurrences)
        {
            return invalid(
                "Calendar repeat series occurrence count exceeds the maximum."
                );
        }

        const std::optional<GregorianDate> occurrenceEnd = fromOrdinal(
            toOrdinal(occurrenceStart) + durationDays
            );
        if (!occurrenceEnd)
        {
            return invalid(
                "Calendar repeat occurrence end date is outside the supported range."
                );
        }

        CalendarEventSaveRequest occurrence = seed;
        occurrence.id.reset();
        occurrence.startDate = formatDate(occurrenceStart);
        occurrence.endDate = formatDate(*occurrenceEnd);
        request.occurrences.push_back(std::move(occurrence));

        const std::optional<GregorianDate> nextStart =
            nextOccurrenceDate(occurrenceStart, frequency);
        if (!nextStart || toOrdinal(*nextStart) > lastOrdinal)
        {
            break;
        }
        occurrenceStart = *nextStart;
    }

    const Domain::Result<void> requestValidation = request.validate();
    if (!requestValidation)
    {
        return invalid(requestValidation.error().message);
    }

    return Domain::Result<CalendarEventSeriesCreateRequest>::success(
        std::move(request)
        );
}

} // namespace ClassMngr::Next::Application
