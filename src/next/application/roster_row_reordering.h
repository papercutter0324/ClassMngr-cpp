#pragma once

#include "next/application/qt_compatible_text.h"
#include "next/application/roster_snapshot.h"

#include <cstddef>
#include <variant>
#include <utility>

namespace ClassMngr::Next::Application
{

enum class RosterRowReorderingErrorCode
{
    InvalidSourceIndex,
    InvalidDestinationIndex,
    SameRow,
    SourceRowHasNoData
};

struct RosterRowReorderingError final
{
    RosterRowReorderingErrorCode code;

    friend bool operator==(
        const RosterRowReorderingError&,
        const RosterRowReorderingError&
        ) = default;
};

using RosterRowReorderingResult =
    std::variant<RosterSnapshot, RosterRowReorderingError>;

// Reorders a complete row while keeping the snapshot's other fields intact.
// Blank-row eligibility follows QString::trimmed().isEmpty() semantics without
// depending on Qt at the Application boundary.
[[nodiscard]] inline RosterRowReorderingResult reorderRosterRows(
    const RosterSnapshot& roster,
    const int sourceRow,
    const int destinationRow
    )
{
    const std::size_t rowCount = roster.rows.size();
    if (sourceRow < 0 || static_cast<std::size_t>(sourceRow) >= rowCount)
    {
        return RosterRowReorderingError{
            RosterRowReorderingErrorCode::InvalidSourceIndex
        };
    }

    if (
        destinationRow < 0
        || static_cast<std::size_t>(destinationRow) >= rowCount
        )
    {
        return RosterRowReorderingError{
            RosterRowReorderingErrorCode::InvalidDestinationIndex
        };
    }

    if (sourceRow == destinationRow)
    {
        return RosterRowReorderingError{
            RosterRowReorderingErrorCode::SameRow
        };
    }

    const auto& source = roster.rows[static_cast<std::size_t>(sourceRow)];
    bool sourceHasData = false;
    for (const std::u16string& cell : source)
    {
        for (const char16_t character : cell)
        {
            if (!isQtWhitespace(character))
            {
                sourceHasData = true;
                break;
            }
        }
        if (sourceHasData)
        {
            break;
        }
    }

    if (!sourceHasData)
    {
        return RosterRowReorderingError{
            RosterRowReorderingErrorCode::SourceRowHasNoData
        };
    }

    RosterSnapshot reordered = roster;
    auto& rows = reordered.rows;
    auto movedRow = std::move(rows[static_cast<std::size_t>(sourceRow)]);
    rows.erase(rows.begin() + sourceRow);
    rows.insert(
        rows.begin() + destinationRow,
        std::move(movedRow)
        );

    return reordered;
}

} // namespace ClassMngr::Next::Application
