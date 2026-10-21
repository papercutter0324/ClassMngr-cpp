#pragma once

namespace ClassMngr::Next::Application
{

// Initial Setup requires the UI-trimmed personal name to contain a value.
[[nodiscard]] constexpr bool initialSetupPersonalNameRequirementMet(
    bool trimmedNamePresent
    ) noexcept
{
    return trimmedNamePresent;
}

} // namespace ClassMngr::Next::Application
