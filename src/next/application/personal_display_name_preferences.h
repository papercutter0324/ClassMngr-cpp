#pragma once

#include "next/domain/operation_result.h"

#include <string>

namespace ClassMngr::Next::Application
{

using PersonalDisplayNamePreferencesSaveResult = Domain::Result<void>;

// The application boundary carries the personal display name as opaque
// UTF-8. Empty and unavailable persistence are represented by an empty
// string; caller-specific trimming remains at the UI boundary.
class PersonalDisplayNamePreferencesPort
{
public:
    virtual ~PersonalDisplayNamePreferencesPort() = default;

    [[nodiscard]] virtual std::string read() const = 0;

    [[nodiscard]] virtual PersonalDisplayNamePreferencesSaveResult write(
        const std::string& name
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
