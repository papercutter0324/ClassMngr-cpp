#pragma once

#include "next/application/roster_custom_column_name_policy.h"
#include "next/application/roster_snapshot.h"

#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace ClassMngr::Next::Application
{

struct RosterCustomColumnAppendError final
{
    RosterCustomColumnNameRejection rejection;
    std::u16string normalizedName;

    friend bool operator==(
        const RosterCustomColumnAppendError&,
        const RosterCustomColumnAppendError&
        ) = default;
};

using RosterCustomColumnAppendResult =
    std::variant<RosterSnapshot, RosterCustomColumnAppendError>;

// Appends an admitted custom-column name and one empty cell per existing row.
// Existing roster cells and column-width metadata are preserved.
template <typename CaseInsensitiveEquals>
[[nodiscard]] inline RosterCustomColumnAppendResult appendRosterCustomColumn(
    const RosterSnapshot& roster,
    const std::u16string_view name,
    const std::vector<std::u16string>& requiredColumnNames,
    CaseInsensitiveEquals&& caseInsensitiveEquals
    )
{
    const RosterCustomColumnNameAdmission admission =
        admitRosterCustomColumnName(
            name,
            roster.columns,
            requiredColumnNames,
            caseInsensitiveEquals
            );
    if (!admission.accepted())
    {
        return RosterCustomColumnAppendError{
            .rejection = *admission.rejection,
            .normalizedName = admission.normalizedName
        };
    }

    RosterSnapshot appended = roster;
    appended.columns.push_back(admission.normalizedName);
    for (std::vector<std::u16string>& row : appended.rows)
    {
        row.emplace_back();
    }
    return appended;
}

} // namespace ClassMngr::Next::Application
