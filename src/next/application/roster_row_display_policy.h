#pragma once

#include <algorithm>
#include <optional>

namespace ClassMngr::Next::Application
{

inline constexpr int DefaultMaximumRosterRows = 25;
inline constexpr int MinimumMaximumRosterRows = 1;
inline constexpr int MaximumRosterRows = 100;
inline constexpr int RosterRowsConfirmationThreshold = 40;

enum class RosterRowDisplayMode : int
{
    AllRows = 0,
    Extra5 = 5,
    Extra10 = 10,
    Extra15 = 15,
    Extra20 = 20,
    Extra25 = 25
};

[[nodiscard]] constexpr bool isValidMaximumRosterRows(const int rows) noexcept
{
    return rows >= MinimumMaximumRosterRows && rows <= MaximumRosterRows;
}

[[nodiscard]] constexpr bool isValidRosterRowDisplayMode(
    const int mode
    ) noexcept
{
    switch (mode)
    {
    case static_cast<int>(RosterRowDisplayMode::AllRows):
    case static_cast<int>(RosterRowDisplayMode::Extra5):
    case static_cast<int>(RosterRowDisplayMode::Extra10):
    case static_cast<int>(RosterRowDisplayMode::Extra15):
    case static_cast<int>(RosterRowDisplayMode::Extra20):
    case static_cast<int>(RosterRowDisplayMode::Extra25):
        return true;
    default:
        return false;
    }
}

[[nodiscard]] constexpr bool shouldConfirmMaximumRosterRowsChange(
    const int previouslySavedRows,
    const int candidateRows
    ) noexcept
{
    return isValidMaximumRosterRows(previouslySavedRows)
        && isValidMaximumRosterRows(candidateRows)
        && previouslySavedRows <= DefaultMaximumRosterRows
        && candidateRows > RosterRowsConfirmationThreshold;
}

// lastNonBlankRowCount is one-based: zero means the roster has no populated
// rows. An extra-row policy always leaves at least one row visible.
[[nodiscard]] constexpr int visibleRosterRowCount(
    const int maximumRows,
    const RosterRowDisplayMode mode,
    const int lastNonBlankRowCount
    ) noexcept
{
    if (!isValidMaximumRosterRows(maximumRows))
    {
        return DefaultMaximumRosterRows;
    }
    if (mode == RosterRowDisplayMode::AllRows)
    {
        return maximumRows;
    }

    const int extraRows = static_cast<int>(mode);
    if (!isValidRosterRowDisplayMode(extraRows))
    {
        return maximumRows;
    }

    if (lastNonBlankRowCount <= 0)
    {
        return std::min(maximumRows, extraRows);
    }

    const int contentRows = lastNonBlankRowCount;
    return std::min(maximumRows, contentRows + extraRows);
}

} // namespace ClassMngr::Next::Application
