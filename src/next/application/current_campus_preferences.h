#pragma once

#include <string>

namespace ClassMngr::Next::Application
{

// The application boundary carries the current campus as an opaque UTF-8
// string. Empty and unavailable persistence are represented by an empty
// string; campus interpretation remains with the calendar UI.
class CurrentCampusPreferencesPort
{
public:
    virtual ~CurrentCampusPreferencesPort() = default;

    [[nodiscard]] virtual std::string read() const = 0;
};

} // namespace ClassMngr::Next::Application
