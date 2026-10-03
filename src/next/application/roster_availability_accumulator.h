#pragma once

#include "next/application/qt_compatible_text.h"
#include "next/application/roster_column_projection.h"
#include "next/application/roster_row_availability.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

class RosterAvailabilityAccumulator final
{
public:
    template <typename CaseInsensitiveEquals>
    explicit RosterAvailabilityAccumulator(
        const std::vector<std::u16string>& storedColumnNames,
        const std::vector<std::u16string>& baseColumnNames,
        CaseInsensitiveEquals&& caseInsensitiveEquals
        )
        : m_retainedSourceColumns(storedColumnNames.size(), false)
    {
        const RosterColumnProjection projection =
            RosterColumnProjection::create(
                storedColumnNames,
                baseColumnNames,
                std::forward<CaseInsensitiveEquals>(caseInsensitiveEquals)
                );
        for (const RosterProjectedColumn& column : projection.columns())
        {
            if (column.sourceColumnIndex)
            {
                m_retainedSourceColumns[*column.sourceColumnIndex] = true;
            }
        }
    }

    void observeCell(
        const int rowIndex,
        const int sourceColumnIndex,
        const std::u16string_view value
        ) noexcept
    {
        if (rowIndex < 0
            || static_cast<std::size_t>(rowIndex) >= RosterModeledRowCount
            || sourceColumnIndex < 0
            || static_cast<std::size_t>(sourceColumnIndex)
                >= m_retainedSourceColumns.size()
            || !m_retainedSourceColumns[
                static_cast<std::size_t>(sourceColumnIndex)
                ])
        {
            return;
        }

        for (const char16_t character : value)
        {
            if (!isQtWhitespace(character))
            {
                m_rows[static_cast<std::size_t>(rowIndex)] = true;
                return;
            }
        }
    }

    // Returns -1 when all modeled rows contain data.
    [[nodiscard]] int firstEmptyRow() const noexcept
    {
        for (std::size_t row = 0; row < m_rows.size(); ++row)
        {
            if (!m_rows[row])
            {
                return static_cast<int>(row);
            }
        }
        return -1;
    }

private:
    std::vector<bool> m_retainedSourceColumns;
    std::array<bool, RosterModeledRowCount> m_rows{};
};

} // namespace ClassMngr::Next::Application
