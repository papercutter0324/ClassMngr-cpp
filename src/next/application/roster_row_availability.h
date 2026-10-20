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

class RosterFirstEmptyRowIndex final
{
public:
    constexpr RosterFirstEmptyRowIndex() noexcept = default;

    explicit constexpr RosterFirstEmptyRowIndex(
        const std::size_t value
        ) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr std::size_t rawValue() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const RosterFirstEmptyRowIndex&,
        const RosterFirstEmptyRowIndex&
        ) = default;

private:
    std::size_t m_value = 0;
};

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
[[nodiscard]] inline RosterFirstEmptyRowIndex firstEmptyRosterRow(
    const std::vector<std::vector<std::u16string>>& rows
    )
{
    for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
        if (!rosterRowHasData(rows[rowIndex]))
        {
            return RosterFirstEmptyRowIndex{rowIndex};
        }
    }
    return RosterFirstEmptyRowIndex{rows.size()};
}

} // namespace ClassMngr::Next::Application
