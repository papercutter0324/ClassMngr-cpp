#pragma once

#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

// The application boundary carries the canonical persisted theme value.
// Storage and legacy representation conversion belong to the adapter.
enum class Theme
{
    Dark = 0,
    Light = 1,
    SystemDefault = 2
};

class ThemePreferencesPort
{
public:
    virtual ~ThemePreferencesPort() = default;

    [[nodiscard]] virtual Theme read() const = 0;
    [[nodiscard]] virtual Domain::Result<void> write(Theme theme) const = 0;
};

} // namespace ClassMngr::Next::Application
