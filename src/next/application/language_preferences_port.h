#pragma once

#include "next/application/user_preferences_state.h"

#include <functional>

namespace ClassMngr::Next::Application
{

// The application boundary carries the existing typed language vocabulary.
// Persistence and legacy value migration belong to the platform adapter.
class LanguagePreferencesPort
{
public:
    virtual ~LanguagePreferencesPort() = default;

    [[nodiscard]] virtual LanguagePreference read() const = 0;

    [[nodiscard]] virtual Domain::Result<void> write(
        LanguagePreference preference
        ) const = 0;

    virtual void clear() const = 0;

    // Capture storage before an action writes. Platform adapters can preserve
    // the exact stored representation, including a previously absent key.
    [[nodiscard]] virtual std::function<Domain::Result<void>()> restorePoint() const
    {
        const auto previous = read();
        return [this, previous]() { return write(previous); };
    }
};

} // namespace ClassMngr::Next::Application
