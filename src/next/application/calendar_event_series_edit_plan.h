#pragma once

#include "next/application/calendar_event_series_edit_port.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

// The platform adapter supplies the occurrence identity and persisted start
// date in repository order. The planner deliberately does not select or sort
// occurrences; the repository query already returns the requested suffix.
struct CalendarEventSeriesEditOccurrenceSnapshot final
{
    int eventId = -1;
    std::string startDate;

    friend bool operator==(
        const CalendarEventSeriesEditOccurrenceSnapshot&,
        const CalendarEventSeriesEditOccurrenceSnapshot&
        ) = default;
};

// A complete set of values for updating one selected occurrence. The ID lets
// the adapter verify that each update is applied to its original event.
struct CalendarEventSeriesEditOccurrenceUpdate final
{
    int eventId = -1;
    std::string startDate;
    std::string endDate;
    std::string repeatSeriesId;
    std::string title;
    std::string eventType;
    std::string timeStatus;
    std::optional<std::string> startTime;
    std::optional<std::string> endTime;
    bool allDay = false;

    friend bool operator==(
        const CalendarEventSeriesEditOccurrenceUpdate&,
        const CalendarEventSeriesEditOccurrenceUpdate&
        ) = default;
};

using CalendarEventSeriesEditPlan =
    std::vector<CalendarEventSeriesEditOccurrenceUpdate>;
using CalendarEventSeriesEditPlanResult =
    Domain::Result<CalendarEventSeriesEditPlan>;

namespace CalendarEventSeriesEditPlanDetail
{

using DayNumber = std::int64_t;

[[nodiscard]] constexpr bool isLeapYear(
    const int year
    ) noexcept
{
    return year % 400 == 0 || (year % 4 == 0 && year % 100 != 0);
}

[[nodiscard]] constexpr DayNumber daysBeforeYear(
    const int year
    ) noexcept
{
    const DayNumber completedYears = year - 1;
    return completedYears * 365
        + completedYears / 4
        - completedYears / 100
        + completedYears / 400;
}

[[nodiscard]] inline DayNumber dayNumber(
    const std::string_view date
    ) noexcept
{
    if (!Domain::CalendarEventTiming::isCanonicalDate(date))
    {
        return -1;
    }

    const int year =
        (date.at(0) - '0') * 1000
        + (date.at(1) - '0') * 100
        + (date.at(2) - '0') * 10
        + (date.at(3) - '0');
    const int month =
        (date.at(5) - '0') * 10
        + (date.at(6) - '0');
    const int day =
        (date.at(8) - '0') * 10
        + (date.at(9) - '0');
    constexpr std::array<int, 12> daysBeforeMonth{
        0, 31, 59, 90, 120, 151,
        181, 212, 243, 273, 304, 334
    };

    DayNumber result = daysBeforeYear(year)
        + daysBeforeMonth.at(static_cast<std::size_t>(month - 1))
        + day - 1;
    if (month > 2 && isLeapYear(year))
    {
        ++result;
    }

    return result;
}

[[nodiscard]] inline std::string dateFromDayNumber(
    const DayNumber value
    )
{
    int firstYear = 1;
    int afterLastYear = 10000;
    while (firstYear + 1 < afterLastYear)
    {
        const int middleYear = firstYear + (afterLastYear - firstYear) / 2;
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
    constexpr std::array<int, 12> daysInMonth{
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };
    while (month < 12)
    {
        int monthLength = daysInMonth.at(static_cast<std::size_t>(month - 1));
        if (month == 2 && isLeapYear(firstYear))
        {
            ++monthLength;
        }
        if (dayOfYear < monthLength)
        {
            break;
        }

        dayOfYear -= monthLength;
        ++month;
    }

    const int day = dayOfYear + 1;
    std::string result;
    result.reserve(kCalendarEventSeriesEditMaxDateLength);
    result.push_back(static_cast<char>('0' + firstYear / 1000));
    result.push_back(static_cast<char>('0' + firstYear / 100 % 10));
    result.push_back(static_cast<char>('0' + firstYear / 10 % 10));
    result.push_back(static_cast<char>('0' + firstYear % 10));
    result.push_back('-');
    result.push_back(static_cast<char>('0' + month / 10));
    result.push_back(static_cast<char>('0' + month % 10));
    result.push_back('-');
    result.push_back(static_cast<char>('0' + day / 10));
    result.push_back(static_cast<char>('0' + day % 10));
    return result;
}

[[nodiscard]] inline CalendarEventSeriesEditPlanResult technicalFailure(
    const char* message
    )
{
    return CalendarEventSeriesEditPlanResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = message,
        .recoverable = false
    });
}

} // namespace CalendarEventSeriesEditPlanDetail

// Plans date and request-field updates for an already selected repository
// suffix. The selection's order is preserved exactly. Invalid persisted dates
// and shifted dates outside the supported Gregorian range are technical
// failures because they describe source data or an unrepresentable update.
[[nodiscard]] inline CalendarEventSeriesEditPlanResult
planCalendarEventSeriesEditSuffix(
    const CalendarEventSeriesEditRequest& request,
    const std::vector<CalendarEventSeriesEditOccurrenceSnapshot>& occurrences
    )
{
    const Domain::Result<void> validation = request.validate();
    if (!validation)
    {
        return CalendarEventSeriesEditPlanResult::failure(validation.error());
    }

    using namespace CalendarEventSeriesEditPlanDetail;
    const DayNumber selectedStartDate = dayNumber(request.startDate);
    const DayNumber editedStartDate = dayNumber(request.editedStartDate);
    const DayNumber editedEndDate = dayNumber(request.editedEndDate);
    if (selectedStartDate < 0 || editedStartDate < 0 || editedEndDate < 0)
    {
        return CalendarEventSeriesEditPlanResult::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message = "Calendar repeat-series edit dates must be valid canonical ISO dates.",
            .recoverable = false
        });
    }

    const DayNumber startDateOffset = editedStartDate - selectedStartDate;
    const DayNumber durationDays = editedEndDate - editedStartDate;
    const std::string repeatSeriesId(
        CalendarEventSeriesEditRequestDetail::trimAscii(
            request.repeatSeriesId
            )
        );
    constexpr DayNumber minimumDayNumber = 0;
    constexpr DayNumber maximumDayNumber = daysBeforeYear(10000) - 1;

    CalendarEventSeriesEditPlan plan;
    plan.reserve(occurrences.size());
    for (const CalendarEventSeriesEditOccurrenceSnapshot& occurrence : occurrences)
    {
        const DayNumber sourceStartDate = dayNumber(occurrence.startDate);
        if (sourceStartDate < minimumDayNumber
            || sourceStartDate > maximumDayNumber)
        {
            return technicalFailure(
                "A calendar repeat-series occurrence has an invalid start date."
                );
        }

        const DayNumber shiftedStartDate = sourceStartDate + startDateOffset;
        const DayNumber shiftedEndDate = shiftedStartDate + durationDays;
        if (shiftedStartDate < minimumDayNumber
            || shiftedStartDate > maximumDayNumber
            || shiftedEndDate < minimumDayNumber
            || shiftedEndDate > maximumDayNumber)
        {
            return technicalFailure(
                "A calendar repeat-series edit would move an occurrence outside the supported date range."
                );
        }

        plan.push_back({
            .eventId = occurrence.eventId,
            .startDate = dateFromDayNumber(shiftedStartDate),
            .endDate = dateFromDayNumber(shiftedEndDate),
            .repeatSeriesId = repeatSeriesId,
            .title = request.title,
            .eventType = request.eventType,
            .timeStatus = request.timeStatus,
            .startTime = request.startTime,
            .endTime = request.endTime,
            .allDay = request.allDay
        });
    }

    return CalendarEventSeriesEditPlanResult::success(std::move(plan));
}

} // namespace ClassMngr::Next::Application
