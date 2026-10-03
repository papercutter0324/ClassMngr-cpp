#pragma once

#include "next/application/qt_compatible_text.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

// RosterModel displays this many rows, and transfer capacity uses the same
// modeled range.
inline constexpr std::size_t RosterModeledRowCount = 25;

[[nodiscard]] inline bool rosterRowHasData(
    const std::vector<std::u16string>& row
    )
{
    for (const std::u16string& cell : row)
    {
        if (!trimQtWhitespace(cell).empty())
        {
            return true;
        }
    }
    return false;
}

// Returns rows.size() when every row contains data, including an empty input.
[[nodiscard]] inline std::size_t firstEmptyRosterRow(
    const std::vector<std::vector<std::u16string>>& rows
    )
{
    for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
        if (!rosterRowHasData(rows[rowIndex]))
        {
            return rowIndex;
        }
    }
    return rows.size();
}

} // namespace ClassMngr::Next::Application
