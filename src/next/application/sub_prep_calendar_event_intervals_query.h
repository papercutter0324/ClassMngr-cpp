#pragma once

#include "next/application/calendar_event_query_port.h"

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

struct SubPrepCalendarEventInterval final
{
    std::string eventType;
    CalendarEventDate startDate;
    CalendarEventDate endDate;

    friend bool operator==(
        const SubPrepCalendarEventInterval&,
        const SubPrepCalendarEventInterval&
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
using SubPrepCalendarEventIntervalsReadResult =
    Domain::Result<SubPrepCalendarEventIntervals>;
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
    const std::string& value = date.value();
    if (value.size() != 10
        || value[4] != '-'
        || value[7] != '-')
    {
        return std::nullopt;
    }

    const auto digit = [&value](const std::size_t index)
    {
        const char character = value[index];
        return character >= '0' && character <= '9'
            ? character - '0'
            : -1;
    };
    int year = 0;
    int month = 0;
    int day = 0;
    for (std::size_t index = 0; index < 4; ++index)
    {
        const int valueDigit = digit(index);
        if (valueDigit < 0)
        {
            return std::nullopt;
        }
        year = year * 10 + valueDigit;
    }
    for (const std::size_t index : {5U, 6U})
    {
        const int valueDigit = digit(index);
        if (valueDigit < 0)
        {
            return std::nullopt;
        }
        month = month * 10 + valueDigit;
    }
    for (const std::size_t index : {8U, 9U})
    {
        const int valueDigit = digit(index);
        if (valueDigit < 0)
        {
            return std::nullopt;
        }
        day = day * 10 + valueDigit;
    }

    if (year < 1 || year > 9999)
    {
        return std::nullopt;
    }

    const std::chrono::year_month_day parsed{
        std::chrono::year{year},
        std::chrono::month{static_cast<unsigned>(month)},
        std::chrono::day{static_cast<unsigned>(day)}
    };
    if (!parsed.ok())
    {
        return std::nullopt;
    }

    return std::chrono::sys_days{parsed};
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
            for (SubPrepCalendarEventInterval& interval : loaded.value())
            {
                const std::string_view normalizedType =
                    SubPrepCalendarEventIntervalsQueryDetail::trim(
                        interval.eventType
                        );
                if (normalizedType != "Vacation"
                    && normalizedType != "Holiday")
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

                interval.eventType.assign(normalizedType);
                result.push_back(std::move(interval));
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
