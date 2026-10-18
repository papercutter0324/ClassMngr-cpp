#pragma once

#include "next/domain/operation_result.h"

#include <string>

namespace ClassMngr::Next::Application
{

// The application boundary carries the current campus as an opaque UTF-8
// string; isAvailable() separately reports persistence availability. read()
// returns a successful empty string for a missing or empty value and preserves
// that fallback when unavailable. A technical repository read failure is
// returned as an error; write() is a successful no-op when unavailable.
// Campus interpretation remains with the calendar UI.
class CurrentCampusPreferencesPort
{
public:
    virtual ~CurrentCampusPreferencesPort() = default;

    [[nodiscard]] virtual bool isAvailable() const = 0;

    [[nodiscard]] virtual Domain::Result<std::string> read() const = 0;

    [[nodiscard]] virtual Domain::Result<void> write(
        const std::string& campus
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
