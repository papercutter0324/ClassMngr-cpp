#pragma once

#include "next/application/calendar_event_projection.h"

#include <cctype>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace ClassMngr::Next::Application
{

// A copied, adapter-neutral calendar date. The query port deliberately keeps
// date text opaque to the application layer; the persistence adapter validates
// and interprets it at its own boundary.
class CalendarEventDate final
{
public:
    CalendarEventDate() = default;

    explicit CalendarEventDate(
        std::string value
        )
        : m_value(std::move(value))
    {
    }

    explicit CalendarEventDate(
        const char* value
        )
        : m_value(value == nullptr ? std::string{} : value)
    {
    }

    [[nodiscard]] static std::optional<CalendarEventDate> fromString(
        const std::string_view value
        )
    {
        if (value.empty())
        {
            return std::nullopt;
        }

        for (const unsigned char character : value)
        {
            if (std::isspace(character) != 0)
            {
                return std::nullopt;
            }
        }

        return CalendarEventDate(std::string(value));
    }

    [[nodiscard]] const std::string& value() const noexcept
    {
        return m_value;
    }

    [[nodiscard]] bool isValid() const noexcept
    {
        return !m_value.empty();
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_value.empty();
    }

    friend bool operator==(
        const CalendarEventDate&,
        const CalendarEventDate&
        ) = default;

private:
    std::string m_value;
};

struct CalendarEventRangeRequest final
{
    std::string databasePath;
    CalendarEventDate startDate;
    CalendarEventDate endDate;

    friend bool operator==(
        const CalendarEventRangeRequest&,
        const CalendarEventRangeRequest&
        ) = default;
};

struct CalendarEventNextEventRequest final
{
    std::string databasePath;
    CalendarEventDate afterDate;

    friend bool operator==(
        const CalendarEventNextEventRequest&,
        const CalendarEventNextEventRequest&
        ) = default;
};

using CalendarEventProjectionResult =
    Domain::Result<CalendarEventProjection>;
using CalendarEventDateResult = Domain::Result<CalendarEventDate>;
using CalendarEventQueryError = Domain::OperationError;

// Worker-owned calendar reads expose only copied requests and value results.
// Implementations must not retain a request reference after the call returns.
class CalendarEventQueryPort
{
public:
    virtual ~CalendarEventQueryPort() = default;

    [[nodiscard]] virtual CalendarEventProjectionResult loadRange(
        const CalendarEventRangeRequest& request
        ) = 0;

    [[nodiscard]] virtual CalendarEventDateResult findNextEventDate(
        const CalendarEventNextEventRequest& request
        ) = 0;
};

// A factory is shared by the cache but creates one independent port for each
// worker invocation. The port, and any database connection it owns, ends with
// that invocation; the factory itself is stateless configuration only.
class CalendarEventQueryPortFactory
{
public:
    virtual ~CalendarEventQueryPortFactory() = default;

    [[nodiscard]] virtual std::unique_ptr<CalendarEventQueryPort> create()
        const = 0;
};

using CalendarEventQueryFactory = CalendarEventQueryPortFactory;

} // namespace ClassMngr::Next::Application
