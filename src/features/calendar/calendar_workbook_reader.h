#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

namespace CalendarImport
{
struct Cell
{
    int row = 0;
    int column = 0;
    int style = 0;
    QString value;
    QString note;
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
    int firstRow = 0;
    int firstColumn = 0;
    int lastRow = 0;
    int lastColumn = 0;

    [[nodiscard]] bool contains(
        int row,
        int column
        ) const
    {
        return row >= firstRow
            && row <= lastRow
            && column >= firstColumn
            && column <= lastColumn;
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
