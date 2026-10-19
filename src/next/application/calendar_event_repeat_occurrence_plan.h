#pragma once

#include "next/application/calendar_event_series_create_port.h"
#include "next/domain/domain_types.h"
#include "next/domain/gregorian_date.h"

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

using GregorianDate = Domain::GregorianDate;
using DayNumber = GregorianDate::DayNumber;

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
        return GregorianDate::fromDayNumber(current.dayNumber() + 1);

    case CalendarEventRepeatFrequency::Weekly:
        return GregorianDate::fromDayNumber(current.dayNumber() + 7);

    case CalendarEventRepeatFrequency::Monthly:
    {
        int year = current.year();
        int month = current.month() + 1;
        if (month > 12)
        {
            month = 1;
            ++year;
        }
        if (year > 9'999)
        {
            return std::nullopt;
        }
        return GregorianDate::fromComponents(
            year,
            month,
            std::min(current.day(), GregorianDate::daysInMonth(year, month))
            );
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

[[nodiscard]] inline std::optional<std::string>
defaultCalendarEventRepeatUntilDate(
    const std::string_view date
    )
{
    using namespace CalendarEventRepeatOccurrencePlanDetail;

    const std::optional<GregorianDate> parsedDate =
        GregorianDate::fromIsoString(date);
    if (!parsedDate)
    {
        return std::nullopt;
    }

    const std::optional<GregorianDate> finalDayOfYear =
        GregorianDate::fromComponents(parsedDate->year(), 12, 31);
    if (!finalDayOfYear)
    {
        return std::nullopt;
    }

    const int daysBack =
        (finalDayOfYear->weekday() - parsedDate->weekday() + 7) % 7;
    const std::optional<GregorianDate> defaultUntilDate =
        GregorianDate::fromDayNumber(
            finalDayOfYear->dayNumber() - daysBack
            );
    if (!defaultUntilDate || defaultUntilDate->year() != parsedDate->year())
    {
        return std::nullopt;
    }

    return defaultUntilDate->toIsoString();
}

[[nodiscard]] inline Domain::Result<CalendarEventSeriesCreateRequest>
planCalendarEventRepeatOccurrences(
    const CalendarEventSaveRequest& seed,
    std::optional<Domain::CalendarEventSeriesId> repeatSeriesId,
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

    const std::optional<GregorianDate> startDate =
        GregorianDate::fromIsoString(seed.startDate);
    const std::optional<GregorianDate> seedEndDate =
        GregorianDate::fromIsoString(seed.endDate);
    const std::optional<GregorianDate> finalStartDate =
        GregorianDate::fromIsoString(untilDate);
    if (!startDate || !seedEndDate || !finalStartDate)
    {
        return invalid(
            "Calendar repeat seed and until dates must be canonical ISO dates."
            );
    }

    const DayNumber firstDayNumber = startDate->dayNumber();
    const DayNumber lastDayNumber = finalStartDate->dayNumber();
    if (lastDayNumber < firstDayNumber)
    {
        return invalid(
            "Calendar repeat until date must not precede the seed start date."
            );
    }

    const DayNumber durationDays = seedEndDate->dayNumber() - firstDayNumber;
    CalendarEventSeriesCreateRequest request;
    request.repeatSeriesId = std::move(repeatSeriesId);

    GregorianDate occurrenceStart = *startDate;
    while (occurrenceStart.dayNumber() <= lastDayNumber)
    {
        if (request.occurrences.size()
            >= kCalendarEventSeriesCreateMaxOccurrences)
        {
            return invalid(
                "Calendar repeat series occurrence count exceeds the maximum."
                );
        }

        const std::optional<GregorianDate> occurrenceEnd =
            GregorianDate::fromDayNumber(
                occurrenceStart.dayNumber() + durationDays
                );
        if (!occurrenceEnd)
        {
            return invalid(
                "Calendar repeat occurrence end date is outside the supported range."
                );
        }

        CalendarEventSaveRequest occurrence = seed;
        occurrence.id.reset();
        occurrence.startDate = occurrenceStart.toIsoString();
        occurrence.endDate = occurrenceEnd->toIsoString();
        request.occurrences.push_back(std::move(occurrence));

        const std::optional<GregorianDate> nextStart =
            nextOccurrenceDate(occurrenceStart, frequency);
        if (!nextStart || nextStart->dayNumber() > lastDayNumber)
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
