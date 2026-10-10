#pragma once

#include <QAbstractItemModel>
#include <QTableView>

inline QModelIndex staffDirectoryCell(
    const QTableView& table,
    const int row,
    const int column
    )
{
    return table.model()->index(row, column);
}

inline QVariant staffDirectoryCellData(
    const QTableView& table,
    const int row,
    const int column,
    const int role = Qt::EditRole
    )
{
    return table.model()->data(staffDirectoryCell(table, row, column), role);
}

inline QString staffDirectoryCellText(
    const QTableView& table,
    const int row,
    const int column,
    const int role = Qt::EditRole
    )
{
    return staffDirectoryCellData(table, row, column, role).toString();
}

inline bool setStaffDirectoryCellText(
    QTableView& table,
    const int row,
    const int column,
    const QString& text
    )
{
    return table.model()->setData(
        staffDirectoryCell(table, row, column),
        text,
        Qt::EditRole);
}

