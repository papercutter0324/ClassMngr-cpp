#pragma once

#include <string>

namespace ClassMngr::Next::Application
{

// The application boundary carries the personal display name as opaque
// UTF-8. Empty and unavailable persistence are represented by an empty
// string; caller-specific trimming remains at the UI boundary.
class PersonalDisplayNamePreferencesPort
{
public:
    virtual ~PersonalDisplayNamePreferencesPort() = default;

    [[nodiscard]] virtual std::string read() const = 0;
};

} // namespace ClassMngr::Next::Application
