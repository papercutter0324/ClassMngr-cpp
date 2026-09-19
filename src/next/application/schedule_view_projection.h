#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

// These limits keep one visible schedule snapshot compact. A query or
// adapter owner must paginate or stage a larger visible scope before calling
// create(); this contract never accepts an unbounded schedule or workbook.
inline constexpr std::size_t kScheduleViewMaxRows = 4'096;
inline constexpr std::size_t kScheduleViewMaxCells = 32'768;
inline constexpr std::size_t kScheduleViewMaxCellsPerRow = 96;
inline constexpr std::size_t kScheduleViewMaxIdentifierLength = 256;
inline constexpr std::size_t kScheduleViewMaxDayLabelLength = 64;
inline constexpr std::size_t kScheduleViewMaxRowLabelLength = 128;
inline constexpr std::size_t kScheduleViewMaxStartTextLength = 32;
inline constexpr std::size_t kScheduleViewMaxEndTextLength = 32;
inline constexpr std::size_t kScheduleViewMaxMeetingTextLength = 256;
inline constexpr std::size_t kScheduleViewMaxRoomTextLength = 256;

enum class ScheduleViewMode
{
    Regular,
    Intensive
};

// A schedule cell is display/navigation metadata only. An absent class or
// teacher reference is an explicit empty or missing-record fallback; this
// projection does not own or resolve a class/teacher record graph.
struct ScheduleViewCell final
{
    std::int32_t order = 0;
    std::int32_t slot = 0;
    std::optional<Domain::ClassId> classId;
    std::optional<Domain::TeacherId> teacherId;
    std::string startText;
    std::string endText;
    std::string meetingText;
    std::string roomText;
    ScheduleViewMode mode = ScheduleViewMode::Regular;

    [[nodiscard]] bool hasClass() const noexcept
    {
        return classId.has_value();
    }

    [[nodiscard]] bool hasTeacher() const noexcept
    {
        return teacherId.has_value();
    }

    [[nodiscard]] bool isEmpty() const noexcept
    {
        return !classId.has_value()
            && !teacherId.has_value()
            && meetingText.empty()
            && roomText.empty();
    }

    friend bool operator==(
        const ScheduleViewCell&,
        const ScheduleViewCell&
        ) = default;
};

// A row is a visible day/time band. Empty rows are valid so an adapter can
// preserve an intentionally empty visible scope without manufacturing cells.
struct ScheduleViewRow final
{
    std::int32_t order = 0;
    std::int32_t day = 0;
    std::string dayLabel;
    std::string rowLabel;
    std::vector<ScheduleViewCell> cells;

    [[nodiscard]] bool empty() const noexcept
    {
        return cells.empty();
    }

    friend bool operator==(
        const ScheduleViewRow&,
        const ScheduleViewRow&
        ) = default;
};

struct ScheduleViewProjectionInput final
{
    std::vector<ScheduleViewRow> rows;

    friend bool operator==(
        const ScheduleViewProjectionInput&,
        const ScheduleViewProjectionInput&
        ) = default;
};

using ScheduleViewInput = ScheduleViewProjectionInput;

namespace ScheduleViewProjectionDetail
{

[[nodiscard]] inline bool isBlank(
    std::string_view value
    ) noexcept
{
    return value.empty()
        || std::all_of(
            value.cbegin(),
            value.cend(),
            [](const char character)
            {
                return std::isspace(
                    static_cast<unsigned char>(character)
                    ) != 0;
            }
            );
}

[[nodiscard]] inline bool isRequiredText(
    const std::string& value,
    const std::size_t maxLength
    ) noexcept
{
    return !isBlank(value) && value.size() <= maxLength;
}

[[nodiscard]] inline bool isOptionalText(
    const std::string& value,
    const std::size_t maxLength
    ) noexcept
{
    return value.empty()
        || (!isBlank(value) && value.size() <= maxLength);
}

[[nodiscard]] inline bool isValidIdentifier(
    std::string_view value
    ) noexcept
{
    return !isBlank(value)
        && value.size() <= kScheduleViewMaxIdentifierLength;
}

template <typename TypedId>
[[nodiscard]] inline bool isValidId(
    const TypedId& id
    ) noexcept
{
    return isValidIdentifier(id.value());
}

[[nodiscard]] inline Domain::OperationError invalidInput(
    const char* message
    )
{
    return Domain::OperationError{
        .code = Domain::ErrorCode::InvalidInput,
        .message = message,
        .recoverable = false
    };
}

[[nodiscard]] inline bool contains(
    const std::vector<std::int32_t>& values,
    const std::int32_t candidate
    ) noexcept
{
    return std::find(values.cbegin(), values.cend(), candidate)
        != values.cend();
}

[[nodiscard]] inline bool isValidMode(
    const ScheduleViewMode mode
    ) noexcept
{
    return mode == ScheduleViewMode::Regular
        || mode == ScheduleViewMode::Intensive;
}

[[nodiscard]] inline Domain::Result<void> validateCell(
    const ScheduleViewCell& cell,
    const std::vector<std::int32_t>& existingOrders,
    const std::vector<std::int32_t>& existingSlots
    )
{
    if (cell.order < 0 || cell.slot < 0)
    {
        return Domain::Result<void>::failure(
            invalidInput("Schedule cell order and slot must not be negative.")
            );
    }

    if (contains(existingOrders, cell.order))
    {
        return Domain::Result<void>::failure(
            invalidInput("Schedule cell orders must be unique within a row.")
            );
    }

    if (contains(existingSlots, cell.slot))
    {
        return Domain::Result<void>::failure(
            invalidInput("Schedule cell slots must be unique within a row.")
            );
    }

    if ((cell.classId.has_value() && !isValidId(*cell.classId))
        || (cell.teacherId.has_value() && !isValidId(*cell.teacherId)))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Optional schedule class and teacher identifiers must be non-blank and bounded."
                )
            );
    }

    if (!isRequiredText(
            cell.startText,
            kScheduleViewMaxStartTextLength
            )
        || !isRequiredText(cell.endText, kScheduleViewMaxEndTextLength)
        || !isOptionalText(
            cell.meetingText,
            kScheduleViewMaxMeetingTextLength
            )
        || !isOptionalText(cell.roomText, kScheduleViewMaxRoomTextLength))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Schedule cell start and end text are required and bounded; meeting and room text must be empty or bounded."
                )
            );
    }

    if (!isValidMode(cell.mode))
    {
        return Domain::Result<void>::failure(
            invalidInput("Schedule cell mode is invalid.")
            );
    }

    return Domain::Result<void>::success();
}

[[nodiscard]] inline Domain::Result<void> validateRow(
    const ScheduleViewRow& row,
    const std::vector<std::int32_t>& existingOrders
    )
{
    if (row.order < 0 || row.day < 0)
    {
        return Domain::Result<void>::failure(
            invalidInput("Schedule row order and day must not be negative.")
            );
    }

    if (contains(existingOrders, row.order))
    {
        return Domain::Result<void>::failure(
            invalidInput("Schedule row orders must be unique.")
            );
    }

    if (!isRequiredText(row.dayLabel, kScheduleViewMaxDayLabelLength)
        || !isRequiredText(row.rowLabel, kScheduleViewMaxRowLabelLength))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Schedule row day and row labels must be non-blank and bounded."
                )
            );
    }

    if (row.cells.size() > kScheduleViewMaxCellsPerRow)
    {
        return Domain::Result<void>::failure(
            invalidInput("Schedule row exceeds its bounded cell limit.")
            );
    }

    std::vector<std::int32_t> cellOrders;
    std::vector<std::int32_t> cellSlots;
    cellOrders.reserve(row.cells.size());
    cellSlots.reserve(row.cells.size());
    for (const auto& cell : row.cells)
    {
        const auto validation = validateCell(cell, cellOrders, cellSlots);
        if (!validation)
        {
            return validation;
        }

        cellOrders.push_back(cell.order);
        cellSlots.push_back(cell.slot);
    }

    return Domain::Result<void>::success();
}

[[nodiscard]] inline Domain::Result<void> validateInput(
    const ScheduleViewProjectionInput& input
    )
{
    if (input.rows.size() > kScheduleViewMaxRows)
    {
        return Domain::Result<void>::failure(
            invalidInput("Schedule view row collection exceeds its bounded limit.")
            );
    }

    std::vector<std::int32_t> rowOrders;
    rowOrders.reserve(input.rows.size());
    std::size_t totalCells = 0;
    for (const auto& row : input.rows)
    {
        const auto validation = validateRow(row, rowOrders);
        if (!validation)
        {
            return validation;
        }

        if (totalCells > kScheduleViewMaxCells - row.cells.size())
        {
            return Domain::Result<void>::failure(
                invalidInput(
                    "Schedule view cell collection exceeds its bounded limit."
                    )
                );
        }

        totalCells += row.cells.size();
        rowOrders.push_back(row.order);
    }

    return Domain::Result<void>::success();
}

}

// This immutable snapshot owns copied visible rows and cells only. The
// adapter/query owner can release rich classes, rosters, repositories, and
// source/workbook data after create() returns. All lookups return value copies.
class ScheduleViewProjection final
{
public:
    using Input = ScheduleViewProjectionInput;
    using Row = ScheduleViewRow;
    using Cell = ScheduleViewCell;

    ScheduleViewProjection() = default;

    [[nodiscard]] static Domain::Result<ScheduleViewProjection> create(
        Input input
        )
    {
        const auto validation = ScheduleViewProjectionDetail::validateInput(
            input
            );
        if (!validation)
        {
            return Domain::Result<ScheduleViewProjection>::failure(
                validation.error()
                );
        }

        std::size_t totalCells = 0;
        for (const auto& row : input.rows)
        {
            totalCells += row.cells.size();
        }

        return Domain::Result<ScheduleViewProjection>::success(
            ScheduleViewProjection(std::move(input.rows), totalCells)
            );
    }

    [[nodiscard]] static Domain::Result<void> validate(
        const Input& input
        )
    {
        return ScheduleViewProjectionDetail::validateInput(input);
    }

    [[nodiscard]] const std::vector<Row>& rows() const noexcept
    {
        return m_rows;
    }

    [[nodiscard]] std::size_t rowCount() const noexcept
    {
        return m_rows.size();
    }

    [[nodiscard]] std::size_t cellCount() const noexcept
    {
        return m_cellCount;
    }

    [[nodiscard]] std::optional<Row> findRow(
        const std::int32_t order
        ) const
    {
        const auto row = std::find_if(
            m_rows.cbegin(),
            m_rows.cend(),
            [order](const Row& candidate)
            {
                return candidate.order == order;
            }
            );
        if (row == m_rows.cend())
        {
            return std::nullopt;
        }

        return *row;
    }

    [[nodiscard]] std::optional<Row> lookupRow(
        const std::int32_t order
        ) const
    {
        return findRow(order);
    }

    [[nodiscard]] std::optional<Cell> findCell(
        const std::int32_t rowOrder,
        const std::int32_t cellOrder
        ) const
    {
        const auto row = std::find_if(
            m_rows.cbegin(),
            m_rows.cend(),
            [rowOrder](const Row& candidate)
            {
                return candidate.order == rowOrder;
            }
            );
        if (row == m_rows.cend())
        {
            return std::nullopt;
        }

        const auto cell = std::find_if(
            row->cells.cbegin(),
            row->cells.cend(),
            [cellOrder](const Cell& candidate)
            {
                return candidate.order == cellOrder;
            }
            );
        if (cell == row->cells.cend())
        {
            return std::nullopt;
        }

        return *cell;
    }

    [[nodiscard]] std::optional<Cell> lookupCell(
        const std::int32_t rowOrder,
        const std::int32_t cellOrder
        ) const
    {
        return findCell(rowOrder, cellOrder);
    }

    [[nodiscard]] std::optional<Cell> findCellBySlot(
        const std::int32_t rowOrder,
        const std::int32_t slot
        ) const
    {
        const auto row = std::find_if(
            m_rows.cbegin(),
            m_rows.cend(),
            [rowOrder](const Row& candidate)
            {
                return candidate.order == rowOrder;
            }
            );
        if (row == m_rows.cend())
        {
            return std::nullopt;
        }

        const auto cell = std::find_if(
            row->cells.cbegin(),
            row->cells.cend(),
            [slot](const Cell& candidate)
            {
                return candidate.slot == slot;
            }
            );
        if (cell == row->cells.cend())
        {
            return std::nullopt;
        }

        return *cell;
    }

    [[nodiscard]] std::optional<Cell> lookupCellBySlot(
        const std::int32_t rowOrder,
        const std::int32_t slot
        ) const
    {
        return findCellBySlot(rowOrder, slot);
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_rows.empty();
    }

    friend bool operator==(
        const ScheduleViewProjection&,
        const ScheduleViewProjection&
        ) = default;

private:
    ScheduleViewProjection(
        std::vector<Row> rows,
        const std::size_t cellCount
        )
        : m_rows(std::move(rows)),
          m_cellCount(cellCount)
    {
    }

    std::vector<Row> m_rows;
    std::size_t m_cellCount = 0;
};

using ScheduleViewSnapshot = ScheduleViewProjection;

} // namespace ClassMngr::Next::Application
