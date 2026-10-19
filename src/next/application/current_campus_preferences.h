#pragma once

#include "next/domain/operation_result.h"

#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

// The selected campus is an opaque, owning value. Bytes are retained exactly;
// interpretation and validation belong to the consuming boundary.
class CurrentCampusPreferenceValue final
{
public:
    CurrentCampusPreferenceValue() = default;

    explicit CurrentCampusPreferenceValue(std::string value)
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] const std::string& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const CurrentCampusPreferenceValue&,
        const CurrentCampusPreferenceValue&
        ) = default;

private:
    std::string m_value;
};

// The application boundary carries the current campus as an opaque UTF-8
// value; isAvailable() separately reports persistence availability. read()
// returns a successful empty value for a missing or empty preference and
// preserves that fallback when unavailable. A technical repository read
// failure is returned as an error; write() is a successful no-op when
// unavailable.
// Interpretation remains with consuming feature boundaries.
class CurrentCampusPreferencesPort
{
public:
    virtual ~CurrentCampusPreferencesPort() = default;

    [[nodiscard]] virtual bool isAvailable() const = 0;

    [[nodiscard]] virtual Domain::Result<CurrentCampusPreferenceValue>
    read() const = 0;

    [[nodiscard]] virtual Domain::Result<void> write(
        const CurrentCampusPreferenceValue& campus
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
