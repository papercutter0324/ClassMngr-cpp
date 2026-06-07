#pragma once

#include <QList>
#include <QStringList>

struct Roster
{
    static const QStringList BaseColumns;

    QStringList columns;
    QList<QStringList> rows;

    void reset();
};