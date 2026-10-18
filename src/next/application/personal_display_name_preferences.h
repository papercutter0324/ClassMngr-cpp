#pragma once

#include "next/domain/operation_result.h"

#include <string>

namespace ClassMngr::Next::Application
{

using PersonalDisplayNamePreferencesSaveResult = Domain::Result<void>;
using PersonalDisplayNamePreferencesReadResult =
    Domain::Result<std::string>;

// The application boundary carries the personal display name as opaque
// UTF-8. A missing, empty, or unavailable preference is a successful empty
// value; technical read failures remain explicit for callers that must avoid
// overwriting an unknown value. Caller-specific trimming stays at the UI
// boundary.
class PersonalDisplayNamePreferencesPort
{
public:
    virtual ~PersonalDisplayNamePreferencesPort() = default;

    [[nodiscard]] virtual PersonalDisplayNamePreferencesReadResult read()
        const = 0;

    [[nodiscard]] virtual PersonalDisplayNamePreferencesSaveResult write(
        const std::string& name
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
