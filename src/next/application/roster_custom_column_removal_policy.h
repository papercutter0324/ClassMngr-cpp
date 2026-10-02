#pragma once

#include "next/application/roster_custom_column_name_policy.h"
#include "next/application/roster_snapshot.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

enum class RosterCustomColumnRemovalRejection
{
    InvalidColumnIndex,
    RequiredColumn
};

struct RosterCustomColumnRemovalEligibility final
{
    std::optional<RosterCustomColumnRemovalRejection> rejection;

    [[nodiscard]] bool accepted() const noexcept
    {
        return !rejection.has_value();
    }
};

// A caller supplies case-insensitive equality so the policy can reuse the
// caller's exact Unicode comparison rules without depending on Qt.
template <typename CaseInsensitiveEquals>
[[nodiscard]] inline RosterCustomColumnRemovalEligibility
canRemoveRosterCustomColumn(
    const RosterSnapshot& roster,
    const int column,
    const std::vector<std::u16string>& requiredColumnNames,
    CaseInsensitiveEquals&& caseInsensitiveEquals
    )
{
    if (column < 0 || static_cast<std::size_t>(column) >= roster.columns.size())
    {
        return {
            .rejection = RosterCustomColumnRemovalRejection::InvalidColumnIndex
        };
    }

    const std::u16string candidate = normalizeRosterCustomColumnName(
        roster.columns[static_cast<std::size_t>(column)],
        caseInsensitiveEquals
        );
    for (const std::u16string& requiredColumn : requiredColumnNames)
    {
        const std::u16string normalizedRequired =
            normalizeRosterCustomColumnName(
                requiredColumn,
                caseInsensitiveEquals
                );
        if (std::invoke(
                caseInsensitiveEquals,
                std::u16string_view(candidate),
                std::u16string_view(normalizedRequired)
                ))
        {
            return {
                .rejection =
                    RosterCustomColumnRemovalRejection::RequiredColumn
            };
        }
    }

    return {.rejection = std::nullopt};
}

} // namespace ClassMngr::Next::Application
