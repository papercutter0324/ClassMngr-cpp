#pragma once

#include "next/application/qt_compatible_text.h"
#include "next/application/roster_availability_first_empty_row.h"
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

class RosterAvailabilityAccumulatorRowIndex final
{
public:
    constexpr RosterAvailabilityAccumulatorRowIndex() noexcept = default;

    explicit constexpr RosterAvailabilityAccumulatorRowIndex(
        const int value
        ) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr int rawValue() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const RosterAvailabilityAccumulatorRowIndex&,
        const RosterAvailabilityAccumulatorRowIndex&
        ) noexcept = default;

private:
    int m_value = 0;
};

class RosterAvailabilityAccumulatorSourceColumnIndex final
{
public:
    constexpr RosterAvailabilityAccumulatorSourceColumnIndex() noexcept =
        default;

    explicit constexpr RosterAvailabilityAccumulatorSourceColumnIndex(
        const int value
        ) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr int rawValue() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const RosterAvailabilityAccumulatorSourceColumnIndex&,
        const RosterAvailabilityAccumulatorSourceColumnIndex&
        ) noexcept = default;

private:
    int m_value = 0;
};

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
                m_retainedSourceColumns[
                    column.sourceColumnIndex->rawValue()
                    ] = true;
            }
        }
    }

    void observeCell(
        const RosterAvailabilityAccumulatorRowIndex rowIndex,
        const RosterAvailabilityAccumulatorSourceColumnIndex sourceColumnIndex,
        const std::u16string_view value
        ) noexcept
    {
        const int rawRowIndex = rowIndex.rawValue();
        const int rawSourceColumnIndex = sourceColumnIndex.rawValue();
        if (rawRowIndex < 0)
        {
            return;
        }
        if (static_cast<std::size_t>(rawRowIndex) >= RosterModeledRowCount)
        {
            return;
        }
        if (rawSourceColumnIndex < 0)
        {
            return;
        }
        const std::size_t sourceColumn =
            static_cast<std::size_t>(rawSourceColumnIndex);
        if (sourceColumn >= m_retainedSourceColumns.size())
        {
            return;
        }
        if (!m_retainedSourceColumns[sourceColumn])
        {
            return;
        }

        for (const char16_t character : value)
        {
            if (!isQtWhitespace(character))
            {
                m_rows[static_cast<std::size_t>(rawRowIndex)] = true;
                return;
            }
        }
    }

    // Returns a full sentinel when all modeled rows contain data.
    [[nodiscard]] RosterAvailabilityFirstEmptyRow firstEmptyRow() const noexcept
    {
        for (std::size_t row = 0; row < m_rows.size(); ++row)
        {
            if (!m_rows[row])
            {
                return RosterAvailabilityFirstEmptyRow(
                    static_cast<int>(row)
                    );
            }
        }
        return RosterAvailabilityFirstEmptyRow(-1);
    }

private:
    std::vector<bool> m_retainedSourceColumns;
    std::array<bool, RosterModeledRowCount> m_rows{};
};

} // namespace ClassMngr::Next::Application
