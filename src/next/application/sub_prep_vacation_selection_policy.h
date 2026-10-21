#pragma once

#include "next/application/sub_prep_calendar_event_intervals_query.h"

#include <algorithm>
#include <cstdint>
#include <set>
#include <vector>

namespace ClassMngr::Next::Application
{

namespace SubPrepVacationSelectionPolicyDetail
{

[[nodiscard]] inline bool isWeekday(const std::int64_t dayNumber) noexcept
{
    return dayNumber % 7 < 5;
}

inline void appendWeekdayDates(
    const Domain::GregorianDate& startDate,
    const Domain::GregorianDate& endDate,
    std::set<std::int64_t>& destination
    )
{
    const std::int64_t lastDay = endDate.dayNumber().rawValue();
    for (
        std::int64_t day = startDate.dayNumber().rawValue();
        day <= lastDay;
        ++day
        )
    {
        if (isWeekday(day))
        {
            destination.insert(day);
        }
    }
}

[[nodiscard]] inline bool datesAreConnected(
    const std::int64_t previousVacationDate,
    const std::int64_t nextVacationDate,
    const std::set<std::int64_t>& holidayDates
    )
{
    for (
        std::int64_t day = previousVacationDate + 1;
        day < nextVacationDate;
        ++day
        )
    {
        if (isWeekday(day) && !holidayDates.contains(day))
        {
            return false;
        }
    }

    return true;
}

[[nodiscard]] inline bool containsEligibleDate(
    const std::vector<std::int64_t>& block,
    const std::int64_t referenceDay
    ) noexcept
{
    const std::int64_t latestEligibleDay = std::min(
        Domain::GregorianDate::maximumDayNumber().rawValue(),
        referenceDay + 28
        );
    return std::any_of(
        block.cbegin(),
        block.cend(),
        [referenceDay, latestEligibleDay](const std::int64_t day)
        {
            return day >= referenceDay && day <= latestEligibleDay;
        }
        );
}

[[nodiscard]] inline std::vector<Domain::GregorianDate> toDates(
    const std::vector<std::int64_t>& dayNumbers
    )
{
    std::vector<Domain::GregorianDate> dates;
    dates.reserve(dayNumbers.size());
    for (const std::int64_t dayNumber : dayNumbers)
    {
        const auto date = Domain::GregorianDate::fromDayNumber(
            Domain::GregorianDate::DayNumber{dayNumber}
            );
        if (date)
        {
            dates.push_back(*date);
        }
    }
    return dates;
}

[[nodiscard]] inline std::vector<Domain::GregorianDate> eligibleBlock(
    const std::vector<std::int64_t>& block,
    const std::int64_t referenceDay
    )
{
    return containsEligibleDate(block, referenceDay)
        ? toDates(block)
        : std::vector<Domain::GregorianDate>{};
}

} // namespace SubPrepVacationSelectionPolicyDetail

// Selects the earliest connected weekday-vacation block that reaches the
// reference date and has at least one vacation day in the inclusive 28-day
// lookahead. The returned block is kept whole, including dates outside it.
[[nodiscard]] inline std::vector<Domain::GregorianDate>
selectNextSubPrepVacationBlock(
    const SubPrepCalendarEventIntervals& intervals,
    const CalendarEventDate& referenceDate
    )
{
    const auto parsedReferenceDate = Domain::GregorianDate::fromIsoString(
        referenceDate.value()
        );
    if (!parsedReferenceDate)
    {
        return {};
    }

    std::set<std::int64_t> vacationDates;
    std::set<std::int64_t> holidayDates;
    for (const SubPrepCalendarEventInterval& interval : intervals)
    {
        if (
            interval.eventType != Domain::CalendarEventType::Vacation
            && interval.eventType != Domain::CalendarEventType::Holiday
            )
        {
            continue;
        }

        const auto startDate = Domain::GregorianDate::fromIsoString(
            interval.startDate.value()
            );
        const auto endDate = Domain::GregorianDate::fromIsoString(
            interval.endDate.value()
            );
        if (
            !startDate
            || !endDate
            || endDate->dayNumber() < startDate->dayNumber()
            )
        {
            continue;
        }

        auto& destination = interval.eventType
                == Domain::CalendarEventType::Vacation
            ? vacationDates
            : holidayDates;
        SubPrepVacationSelectionPolicyDetail::appendWeekdayDates(
            *startDate,
            *endDate,
            destination
            );
    }

    if (vacationDates.empty())
    {
        return {};
    }

    const std::int64_t referenceDay =
        parsedReferenceDate->dayNumber().rawValue();
    std::vector<std::int64_t> block;
    for (const std::int64_t vacationDate : vacationDates)
    {
        if (
            !block.empty()
            && !SubPrepVacationSelectionPolicyDetail::datesAreConnected(
                block.back(),
                vacationDate,
                holidayDates
                )
            )
        {
            if (block.back() >= referenceDay)
            {
                return SubPrepVacationSelectionPolicyDetail::eligibleBlock(
                    block,
                    referenceDay
                    );
            }
            block.clear();
        }

        block.push_back(vacationDate);
    }

    return !block.empty() && block.back() >= referenceDay
        ? SubPrepVacationSelectionPolicyDetail::eligibleBlock(
              block,
              referenceDay
              )
        : std::vector<Domain::GregorianDate>{};
}

} // namespace ClassMngr::Next::Application
