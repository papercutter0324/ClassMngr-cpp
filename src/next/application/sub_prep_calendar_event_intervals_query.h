#pragma once

#include "next/application/calendar_event_query_port.h"
#include "next/domain/calendar_event_timing.h"
#include "next/domain/gregorian_date.h"

#include <chrono>
#include <cctype>
#include <cstddef>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

// A normalized query result with its canonical event type represented in the
// domain model.
struct SubPrepCalendarEventInterval final
{
    Domain::CalendarEventType eventType;
    CalendarEventDate startDate;
    CalendarEventDate endDate;

    friend bool operator==(
        const SubPrepCalendarEventInterval&,
        const SubPrepCalendarEventInterval&
        ) = default;
};

// Raw event-type text from the read boundary; the query classifies it.
struct SubPrepCalendarEventIntervalRead final
{
    std::string eventType;
    CalendarEventDate startDate;
    CalendarEventDate endDate;

    friend bool operator==(
        const SubPrepCalendarEventIntervalRead&,
        const SubPrepCalendarEventIntervalRead&
        ) = default;
};

struct SubPrepCalendarEventIntervalsRequest final
{
    CalendarEventDate referenceDate;

    friend bool operator==(
        const SubPrepCalendarEventIntervalsRequest&,
        const SubPrepCalendarEventIntervalsRequest&
        ) = default;
};

struct SubPrepCalendarEventIntervalsReadRequest final
{
    CalendarEventDate startDate;
    CalendarEventDate endDate;

    friend bool operator==(
        const SubPrepCalendarEventIntervalsReadRequest&,
        const SubPrepCalendarEventIntervalsReadRequest&
        ) = default;
};

using SubPrepCalendarEventIntervals =
    std::vector<SubPrepCalendarEventInterval>;
using SubPrepCalendarEventIntervalReads =
    std::vector<SubPrepCalendarEventIntervalRead>;
using SubPrepCalendarEventIntervalsReadResult =
    Domain::Result<SubPrepCalendarEventIntervalReads>;
using SubPrepCalendarEventIntervalsQueryResult =
    Domain::Result<SubPrepCalendarEventIntervals>;

// The read boundary returns only the values the Sub Prep calendar dialog
// needs. Implementations must not retain the request after the call returns.
class SubPrepCalendarEventIntervalsReadPort
{
public:
    virtual ~SubPrepCalendarEventIntervalsReadPort() = default;

    [[nodiscard]] virtual SubPrepCalendarEventIntervalsReadResult
    loadIntervals(
        const SubPrepCalendarEventIntervalsReadRequest& request
        ) = 0;
};

namespace SubPrepCalendarEventIntervalsQueryDetail
{

[[nodiscard]] inline std::optional<std::chrono::sys_days> parseDate(
    const CalendarEventDate& date
    ) noexcept
{
    const auto parsedDate = Domain::GregorianDate::fromIsoString(date.value());
    if (!parsedDate)
    {
        return std::nullopt;
    }

    const std::chrono::year_month_day calendarDate{
        std::chrono::year{parsedDate->year()},
        std::chrono::month{static_cast<unsigned>(parsedDate->month())},
        std::chrono::day{static_cast<unsigned>(parsedDate->day())}
    };
    return std::chrono::sys_days{calendarDate};
}

[[nodiscard]] inline std::string_view trim(
    const std::string_view value
    ) noexcept
{
    std::size_t first = 0;
    while (first < value.size()
        && std::isspace(static_cast<unsigned char>(value[first])) != 0)
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first
        && std::isspace(static_cast<unsigned char>(value[last - 1])) != 0)
    {
        --last;
    }

    return value.substr(first, last - first);
}

[[nodiscard]] inline std::optional<SubPrepCalendarEventIntervalsReadRequest>
readRequestForReferenceDate(
    const CalendarEventDate& referenceDate
    )
{
    if (!parseDate(referenceDate))
    {
        return std::nullopt;
    }

    int referenceYear = 0;
    const std::string& value = referenceDate.value();
    for (std::size_t index = 0; index < 4; ++index)
    {
        referenceYear = referenceYear * 10 + (value[index] - '0');
    }
    const int endYear = referenceYear == 9999
        ? 9999
        : referenceYear + 1;

    const auto formatYear = [](const int year)
    {
        std::string result = std::to_string(year);
        result.insert(0, 4 - result.size(), '0');
        return result;
    };

    return SubPrepCalendarEventIntervalsReadRequest{
        CalendarEventDate(formatYear(referenceYear) + "-01-01"),
        CalendarEventDate(formatYear(endYear) + "-12-31")
    };
}

[[nodiscard]] inline Domain::OperationError error(
    const Domain::ErrorCode code,
    std::string message
    )
{
    return {
        .code = code,
        .message = std::move(message),
        .recoverable = false
    };
}

} // namespace SubPrepCalendarEventIntervalsQueryDetail

// The operation is scoped to one Sub Prep dialog. It has no cache and does
// not impose a result count limit because every connected date interval may
// affect the dialog's vacation-block calculation.
class SubPrepCalendarEventIntervalsQuery final
{
public:
    explicit SubPrepCalendarEventIntervalsQuery(
        SubPrepCalendarEventIntervalsReadPort& readPort
        ) noexcept
        : m_readPort(readPort)
    {
    }

    [[nodiscard]] SubPrepCalendarEventIntervalsQueryResult execute(
        const SubPrepCalendarEventIntervalsRequest& request
        ) const
    {
        const auto readRequest =
            SubPrepCalendarEventIntervalsQueryDetail::
                readRequestForReferenceDate(request.referenceDate);
        if (!readRequest)
        {
            return SubPrepCalendarEventIntervalsQueryResult::failure(
                SubPrepCalendarEventIntervalsQueryDetail::error(
                    Domain::ErrorCode::InvalidInput,
                    "Sub Prep calendar reference date must be valid."
                    )
                );
        }

        const auto start =
            SubPrepCalendarEventIntervalsQueryDetail::parseDate(
                readRequest->startDate
                );
        const auto end =
            SubPrepCalendarEventIntervalsQueryDetail::parseDate(
                readRequest->endDate
                );
        if (!start || !end)
        {
            return SubPrepCalendarEventIntervalsQueryResult::failure(
                SubPrepCalendarEventIntervalsQueryDetail::error(
                    Domain::ErrorCode::Technical,
                    "Sub Prep calendar window could not be derived."
                    )
                );
        }

        try
        {
            auto loaded = m_readPort.loadIntervals(*readRequest);
            if (!loaded)
            {
                return SubPrepCalendarEventIntervalsQueryResult::failure(
                    loaded.error()
                    );
            }

            SubPrepCalendarEventIntervals result;
            for (
                const SubPrepCalendarEventIntervalRead& interval : loaded.value()
                )
            {
                const std::string_view normalizedType =
                    SubPrepCalendarEventIntervalsQueryDetail::trim(
                        interval.eventType
                        );
                const auto eventType =
                    Domain::calendarEventTypeFromName(normalizedType);
                if (!eventType
                    || (*eventType != Domain::CalendarEventType::Vacation
                        && *eventType != Domain::CalendarEventType::Holiday))
                {
                    continue;
                }

                const auto intervalStart =
                    SubPrepCalendarEventIntervalsQueryDetail::parseDate(
                        interval.startDate
                        );
                const auto intervalEnd =
                    SubPrepCalendarEventIntervalsQueryDetail::parseDate(
                        interval.endDate
                        );
                if (!intervalStart
                    || !intervalEnd
                    || *intervalEnd < *intervalStart
                    || *intervalEnd < *start
                    || *end < *intervalStart)
                {
                    continue;
                }

                result.push_back({
                    .eventType = *eventType,
                    .startDate = interval.startDate,
                    .endDate = interval.endDate
                });
            }

            return SubPrepCalendarEventIntervalsQueryResult::success(
                std::move(result)
                );
        }
        catch (const std::exception&)
        {
            return SubPrepCalendarEventIntervalsQueryResult::failure(
                SubPrepCalendarEventIntervalsQueryDetail::error(
                    Domain::ErrorCode::Technical,
                    "Sub Prep calendar intervals could not be loaded."
                    )
                );
        }
        catch (...)
        {
            return SubPrepCalendarEventIntervalsQueryResult::failure(
                SubPrepCalendarEventIntervalsQueryDetail::error(
                    Domain::ErrorCode::Technical,
                    "Sub Prep calendar intervals could not be loaded."
                    )
                );
        }
    }

private:
    SubPrepCalendarEventIntervalsReadPort& m_readPort;
};

} // namespace ClassMngr::Next::Application
