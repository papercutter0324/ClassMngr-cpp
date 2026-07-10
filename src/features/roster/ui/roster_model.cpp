#include "roster_model.h"

#include "features/roster/ui/roster_constants.h"

RosterModel::RosterModel(
    QObject* parent
    )
    : QAbstractTableModel(parent)
{
    Roster roster;
    setRoster(roster);
}

int RosterModel::rowCount(
    const QModelIndex& parent
    ) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return RosterUi::RowCount;
}

int RosterModel::columnCount(
    const QModelIndex& parent
    ) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return m_columns.size();
}

QVariant RosterModel::data(
    const QModelIndex& index,
    int role
    ) const
{
    if (!index.isValid())
    {
        return {};
    }

    if (
        index.row() < 0
        || index.row() >= m_rows.size()
        || index.column() < 0
        || index.column() >= m_columns.size()
        )
    {
        return {};
    }

    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        return m_rows[index.row()][index.column()];
    }

    if (role == Qt::TextAlignmentRole)
    {
        return Qt::AlignCenter;
    }

    if (role == Qt::ToolTipRole)
    {
        const QStringList errors =
            errorsForCell(
                index.row(),
                index.column()
                );

        if (!errors.isEmpty())
        {
            return errors.join(QLatin1Char('\n'));
        }
    }

    return {};
}

bool RosterModel::setData(
    const QModelIndex& index,
    const QVariant& value,
    int role
    )
{
    if (
        role != Qt::EditRole
        || !index.isValid()
        || index.row() < 0
        || index.row() >= m_rows.size()
        || index.column() < 0
        || index.column() >= m_columns.size()
        )
    {
        return false;
    }

    const QHash<QString, QStringList> oldErrors =
        m_validationErrors;

    const QString normalized =
        normalizeCell(
            value.toString(),
            index.column()
            );

    const bool changed =
        m_rows[index.row()][index.column()] != normalized;

    if (changed)
    {
        m_rows[index.row()][index.column()] =
            normalized;

        m_dirtyCells.insert(
            cellKey(
                index.row(),
                index.column()
                )
            );
    }

    validateCellAt(
        index.row(),
        index.column()
        );
    validateRawInput(
        index.row(),
        index.column(),
        value.toString()
        );
    validateDuplicateNames();

    const bool validationChanged =
        oldErrors != m_validationErrors;

    if (!changed && !validationChanged)
    {
        return false;
    }

    emit dataChanged(
        index,
        index,
        {
            Qt::DisplayRole,
            Qt::EditRole,
            Qt::ToolTipRole
        }
        );

    if (changed)
    {
        setDirty(true);
    }

    return true;
}

Qt::ItemFlags RosterModel::flags(
    const QModelIndex& index
    ) const
{
    Qt::ItemFlags itemFlags =
        QAbstractTableModel::flags(index);

    if (index.isValid())
    {
        const QString column =
            columnName(
                index.column()
                );

        if (!RosterUi::isEvaluationColumn(column))
        {
            itemFlags |= Qt::ItemIsEditable;
        }
    }

    return itemFlags;
}

QVariant RosterModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role
    ) const
{
    if (role != Qt::DisplayRole)
    {
        return {};
    }

    if (orientation == Qt::Horizontal)
    {
        return columnName(section);
    }

    return section + 1;
}

void RosterModel::setRoster(
    const Roster& roster
    )
{
    beginResetModel();

    m_columns =
        Roster::BaseColumns;

    for (const QString& column : roster.columns)
    {
        const QString normalized =
            normalizedColumnName(column);

        if (
            normalized.isEmpty()
            || isRequiredColumn(normalized)
            || findColumn(normalized, m_columns) >= 0
            )
        {
            continue;
        }

        m_columns.append(normalized);
    }

    rebuildRows(roster);
    validateAll();

    m_dirtyCells.clear();
    m_dirty = false;

    endResetModel();

    emit dirtyChanged(false);
}

Roster RosterModel::toRoster() const
{
    Roster roster;
    roster.columns = m_columns;
    roster.rows = m_rows;

    return roster;
}

QStringList RosterModel::columnNames() const
{
    return m_columns;
}

bool RosterModel::isDirty() const
{
    return m_dirty;
}

void RosterModel::clearDirty()
{
    m_dirtyCells.clear();
    setDirty(false);
}

QStringList RosterModel::errorsForCell(
    int row,
    int column
    ) const
{
    return m_validationErrors.value(
        cellKey(row, column)
        );
}

QString RosterModel::cellKey(
    int row,
    int column
    ) const
{
    return QStringLiteral("%1:%2")
        .arg(row)
        .arg(column);
}

void RosterModel::setDirty(
    bool dirty
    )
{
    if (m_dirty == dirty)
    {
        return;
    }

    m_dirty = dirty;
    emit dirtyChanged(m_dirty);
}
