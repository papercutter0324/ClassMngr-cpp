#pragma once

#include "next/application/roster_custom_column_name_policy.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

struct RosterProjectedColumn final
{
    std::u16string name;
    std::optional<std::size_t> sourceColumnIndex;

    friend bool operator==(
        const RosterProjectedColumn&,
        const RosterProjectedColumn&
        ) = default;
};

// Projects ordered stored headers into the columns RosterModel displays.
// The caller supplies the single authoritative base-column list and the
// platform's case-insensitive comparison behavior.
class RosterColumnProjection final
{
public:
    template <typename CaseInsensitiveEquals>
    [[nodiscard]] static RosterColumnProjection create(
        const std::vector<std::u16string>& storedColumnNames,
        const std::vector<std::u16string>& baseColumnNames,
        CaseInsensitiveEquals&& caseInsensitiveEquals
        )
    {
        RosterColumnProjection projection;
        projection.m_sourceColumnCount = storedColumnNames.size();
        projection.m_columns.reserve(
            baseColumnNames.size() + storedColumnNames.size()
            );

        for (const std::u16string& baseColumnName : baseColumnNames)
        {
            projection.m_columns.push_back({
                .name = baseColumnName,
                .sourceColumnIndex = findFirstMatchingStoredColumn(
                    baseColumnName,
                    storedColumnNames,
                    caseInsensitiveEquals
                    )
            });
        }

        for (std::size_t sourceIndex = 0;
             sourceIndex < storedColumnNames.size();
             ++sourceIndex)
        {
            std::u16string normalizedName =
                normalizeRosterCustomColumnName(
                    storedColumnNames[sourceIndex],
                    caseInsensitiveEquals
                    );
            if (normalizedName.empty()
                || matchesAny(
                    normalizedName,
                    baseColumnNames,
                    caseInsensitiveEquals
                    )
                || matchesProjected(
                    normalizedName,
                    projection.m_columns,
                    caseInsensitiveEquals
                    ))
            {
                continue;
            }

            projection.m_columns.push_back({
                .name = std::move(normalizedName),
                .sourceColumnIndex = sourceIndex
            });
        }

        return projection;
    }

    [[nodiscard]] const std::vector<RosterProjectedColumn>& columns() const
        noexcept
    {
        return m_columns;
    }

    [[nodiscard]] std::size_t sourceColumnCount() const noexcept
    {
        return m_sourceColumnCount;
    }

private:
    template <typename CaseInsensitiveEquals>
    [[nodiscard]] static bool equivalent(
        const std::u16string_view left,
        const std::u16string_view right,
        CaseInsensitiveEquals& caseInsensitiveEquals
        )
    {
        const std::u16string normalizedLeft =
            normalizeRosterCustomColumnName(left, caseInsensitiveEquals);
        const std::u16string normalizedRight =
            normalizeRosterCustomColumnName(right, caseInsensitiveEquals);
        return std::invoke(
            caseInsensitiveEquals,
            std::u16string_view(normalizedLeft),
            std::u16string_view(normalizedRight)
            );
    }

    template <typename CaseInsensitiveEquals>
    [[nodiscard]] static std::optional<std::size_t>
    findFirstMatchingStoredColumn(
        const std::u16string_view requestedName,
        const std::vector<std::u16string>& storedColumnNames,
        CaseInsensitiveEquals& caseInsensitiveEquals
        )
    {
        for (std::size_t index = 0; index < storedColumnNames.size(); ++index)
        {
            if (equivalent(
                    requestedName,
                    storedColumnNames[index],
                    caseInsensitiveEquals
                    ))
            {
                return index;
            }
        }
        return std::nullopt;
    }

    template <typename CaseInsensitiveEquals>
    [[nodiscard]] static bool matchesAny(
        const std::u16string_view name,
        const std::vector<std::u16string>& names,
        CaseInsensitiveEquals& caseInsensitiveEquals
        )
    {
        for (const std::u16string& candidate : names)
        {
            if (equivalent(name, candidate, caseInsensitiveEquals))
            {
                return true;
            }
        }
        return false;
    }

    template <typename CaseInsensitiveEquals>
    [[nodiscard]] static bool matchesProjected(
        const std::u16string_view name,
        const std::vector<RosterProjectedColumn>& projectedColumns,
        CaseInsensitiveEquals& caseInsensitiveEquals
        )
    {
        for (const RosterProjectedColumn& candidate : projectedColumns)
        {
            if (equivalent(name, candidate.name, caseInsensitiveEquals))
            {
                return true;
            }
        }
        return false;
    }

    std::size_t m_sourceColumnCount = 0;
    std::vector<RosterProjectedColumn> m_columns;
};

} // namespace ClassMngr::Next::Application
