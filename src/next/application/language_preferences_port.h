#pragma once

#include "next/application/user_preferences_state.h"

namespace ClassMngr::Next::Application
{

// The application boundary carries the existing typed language vocabulary.
// Persistence and legacy value migration belong to the platform adapter.
class LanguagePreferencesPort
{
public:
    virtual ~LanguagePreferencesPort() = default;

    [[nodiscard]] virtual LanguagePreference read() const = 0;

    virtual void write(
        LanguagePreference preference
        ) const = 0;

    virtual void clear() const = 0;
};

} // namespace ClassMngr::Next::Application
