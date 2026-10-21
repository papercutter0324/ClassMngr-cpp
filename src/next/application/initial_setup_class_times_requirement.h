#pragma once

namespace ClassMngr::Next::Application
{

// Initial Setup requires at least one regular or intensive schedule row.
[[nodiscard]] constexpr bool initialSetupClassTimesRequirementMet(
    bool hasRegularRows,
    bool hasIntensiveRows
    ) noexcept
{
    return hasRegularRows || hasIntensiveRows;
}

} // namespace ClassMngr::Next::Application
