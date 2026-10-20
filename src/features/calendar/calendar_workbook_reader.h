#pragma once

#include <QByteArray>
#include <QHashFunctions>
#include <QString>
#include <QStringList>
#include <QVector>

#include <cstddef>
#include <functional>
#include <utility>

namespace CalendarImport
{
struct CellPosition
{
    static constexpr int MaxRow = 1'048'576;
    static constexpr int MaxColumn = 16'384;

    int row = 0;
    int column = 0;

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return row >= 1
            && row <= MaxRow
            && column >= 1
            && column <= MaxColumn;
    }

    friend bool operator==(
        const CellPosition&,
        const CellPosition&
        ) = default;

    friend size_t qHash(
        const CellPosition& position,
        size_t seed = 0
        ) noexcept
    {
        return qHashMulti(seed, position.row, position.column);
    }
};

struct Cell
{
    CellPosition position;
    int style = 0;
    QString value;
    QString note;

    Cell() = default;
    Cell(
        int rowNumber,
        int columnNumber,
        int styleIndex,
        QString cellValue,
        QString cellNote
        )
        : position{rowNumber, columnNumber}
        , style(styleIndex)
        , value(std::move(cellValue))
        , note(std::move(cellNote))
    {
    }
};

struct Style
{
    QString fillColor;
    QString fontColor;
    bool filled = false;
    bool bold = false;
};

struct CellRange
{
    CellPosition first;
    CellPosition last;

    [[nodiscard]] bool isValid() const noexcept
    {
        return first.isValid()
            && last.isValid()
            && last.row >= first.row
            && last.column >= first.column;
    }

    [[nodiscard]] bool contains(
        CellPosition position
        ) const
    {
        return isValid()
            && position.isValid()
            && position.row >= first.row
            && position.row <= last.row
            && position.column >= first.column
            && position.column <= last.column;
    }
};

struct Worksheet
{
    QString name;
    bool visible = true;
    QVector<Cell> cells;
    QVector<CellRange> mergedRanges;
};

struct Workbook
{
    QStringList sharedStrings;
    QVector<Style> styles;
    QVector<Worksheet> worksheets;
};

struct WorkbookMetadata
{
    QStringList sharedStrings;
    QVector<Style> styles;
};

using WorksheetConsumer = std::function<bool(
    const WorkbookMetadata& metadata,
    const Worksheet& worksheet
    )>;

QString normalizedColor(
    QString color
    );

Workbook parseWorkbook(
    const QByteArray& data,
    QString* errorMessage
    );

// Calls the consumer synchronously in workbook order. Metadata is valid for the
// full call; the worksheet and its parsed cells are valid only during the
// callback. The decompressed worksheet XML is released before the callback.
// Returning false stops parsing and reports the generic unreadable-workbook
// error through errorMessage.
bool forEachWorksheet(
    const QByteArray& data,
    const WorksheetConsumer& consumer,
    QString* errorMessage
    );
}
