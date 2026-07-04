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
};

struct Style
{
    QString fillColor;
    QString fontColor;
};

struct Workbook
{
    QStringList sharedStrings;
    QVector<Style> styles;
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
