#pragma once

#include "next/application/qt_compatible_text.h"
#include "next/application/roster_snapshot.h"

#include <cstddef>
#include <utility>
#include <variant>

namespace ClassMngr::Next::Application
{

enum class RosterRowRemovalErrorCode
{
    InvalidRowIndex,
    RowHasNoData
};

struct RosterRowRemovalError final
{
    RosterRowRemovalErrorCode code;

    friend bool operator==(
        const RosterRowRemovalError&,
        const RosterRowRemovalError&
        ) = default;
};

using RosterRowRemovalResult =
    std::variant<RosterSnapshot, RosterRowRemovalError>;

// Removes a populated row by compacting following rows and clearing the final
// slot. Snapshot columns and width metadata remain unchanged.
[[nodiscard]] inline RosterRowRemovalResult removeRosterRow(
    const RosterSnapshot& roster,
    const int row
    )
{
    const std::size_t rowCount = roster.rows.size();
    if (row < 0 || static_cast<std::size_t>(row) >= rowCount)
    {
        return RosterRowRemovalError{
            RosterRowRemovalErrorCode::InvalidRowIndex
        };
    }

    bool rowHasData = false;
    for (const std::u16string& cell : roster.rows[static_cast<std::size_t>(row)])
    {
        for (const char16_t character : cell)
        {
            if (!isQtWhitespace(character))
            {
                rowHasData = true;
                break;
            }
        }
        if (rowHasData)
        {
            break;
        }
    }

    if (!rowHasData)
    {
        return RosterRowRemovalError{
            RosterRowRemovalErrorCode::RowHasNoData
        };
    }

    RosterSnapshot removed = roster;
    auto& rows = removed.rows;
    for (std::size_t sourceRow = static_cast<std::size_t>(row) + 1;
         sourceRow < rowCount;
         ++sourceRow)
    {
        rows[sourceRow - 1] = std::move(rows[sourceRow]);
    }

    rows.back() = std::vector<std::u16string>(roster.columns.size());
    return removed;
}

} // namespace ClassMngr::Next::Application
