#pragma once

#include <QList>
#include <QVector>
#include <QStringList>

struct Roster
{
    static const QStringList BaseColumns;

    QStringList columns;
    QVector<int> columnWidths;
    QList<QStringList> rows;

    void reset();
};
