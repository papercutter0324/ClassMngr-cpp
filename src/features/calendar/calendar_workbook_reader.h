#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

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

    // Compatibility view used by the existing calendar parser.
    QVector<Cell> cells;
};

QString normalizedColor(
    QString color
    );

Workbook parseWorkbook(
    const QByteArray& data,
    QString* errorMessage
    );
}
