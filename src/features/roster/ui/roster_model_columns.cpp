#include "roster_model.h"

#include "features/roster/ui/roster_constants.h"

#include <algorithm>

QString RosterModel::columnName(
    int column
    ) const
{
    if (column < 0 || column >= m_columns.size())
    {
        return {};
    }

    return m_columns[column];
}

QStringList RosterModel::rowValues(
    int row
    ) const
{
    if (row < 0 || row >= m_rows.size())
    {
        return {};
    }

    return m_rows[row];
}

int RosterModel::firstEmptyRow() const
{
    for (int row = 0; row < m_rows.size(); ++row)
    {
        if (!rowHasData(m_rows[row]))
        {
            return row;
        }
    }

    return -1;
}

bool RosterModel::isRequiredColumn(
    int column
    ) const
{
    return isRequiredColumn(
        columnName(column)
        );
}

bool RosterModel::isRequiredColumn(
    const QString& name
    ) const
{
    return RosterUi::isRequiredColumn(name);
}

bool RosterModel::canAddColumn(
    const QString& name,
    QString* reason
    ) const
{
    const QString normalized =
        normalizedColumnName(name);

    if (normalized.isEmpty())
    {
        if (reason)
        {
            *reason = tr("Column name cannot be empty.");
        }

        return false;
    }

    if (findColumn(normalized, m_columns) >= 0)
    {
        if (reason)
        {
            *reason = tr("A column with that name already exists.");
        }

        return false;
    }

    if (isRequiredColumn(normalized))
    {
        if (reason)
        {
            *reason = tr("Required roster columns already exist.");
        }

        return false;
    }

    return true;
}

bool RosterModel::insertCustomColumn(
    const QString& name
    )
{
    QString reason;

    if (!canAddColumn(name, &reason))
    {
        Q_UNUSED(reason);
        return false;
    }

    const QString normalized =
        normalizedColumnName(name);

    const int column =
        m_columns.size();

    beginInsertColumns(
        QModelIndex(),
        column,
        column
        );

    m_columns.append(normalized);

    for (QStringList& row : m_rows)
    {
        row.append(QString());
    }

    endInsertColumns();

    validateAll();
    setDirty(true);

    return true;
}

bool RosterModel::canRemoveColumn(
    int column,
    QString* reason
    ) const
{
    if (column < 0 || column >= m_columns.size())
    {
        if (reason)
        {
            *reason = tr("Select a custom column to remove.");
        }

        return false;
    }

    if (isRequiredColumn(column))
    {
        if (reason)
        {
            *reason = tr("Required roster columns cannot be removed.");
        }

        return false;
    }

    return true;
}

bool RosterModel::removeRosterColumn(
    int column
    )
{
    QString reason;

    if (!canRemoveColumn(column, &reason))
    {
        Q_UNUSED(reason);
        return false;
    }

    beginRemoveColumns(
        QModelIndex(),
        column,
        column
        );

    m_columns.removeAt(column);

    for (QStringList& row : m_rows)
    {
        if (column >= 0 && column < row.size())
        {
            row.removeAt(column);
        }
    }

    endRemoveColumns();

    validateAll();
    setDirty(true);

    return true;
}


QString RosterModel::normalizedColumnName(
    const QString& name
    ) const
{
    const QString normalized =
        name.simplified();

    if (normalized.compare(QStringLiteral("Autumn"), Qt::CaseInsensitive) == 0)
    {
        return QStringLiteral("Fall");
    }

    return normalized;
}

int RosterModel::findColumn(
    const QString& name,
    const QStringList& columns
    ) const
{
    for (int index = 0; index < columns.size(); ++index)
    {
        if (
            normalizedColumnName(columns[index])
                .compare(
                    normalizedColumnName(name),
                    Qt::CaseInsensitive
                    ) == 0
            )
        {
            return index;
        }
    }

    return -1;
}

QStringList RosterModel::mappedTransferRow(
    const QStringList& sourceColumns,
    const QStringList& sourceRow
    ) const
{
    QStringList mappedRow(
        m_columns.size(),
        QString()
        );

    for (int destinationColumn = 0; destinationColumn < m_columns.size(); ++destinationColumn)
    {
        const int sourceColumn =
            findColumn(
                m_columns[destinationColumn],
                sourceColumns
                );

        if (
            sourceColumn < 0
            || sourceColumn >= sourceRow.size()
            )
        {
            continue;
        }

        mappedRow[destinationColumn] =
            normalizeCell(
                sourceRow[sourceColumn],
                destinationColumn
                );
    }

    return mappedRow;
}

bool RosterModel::rowHasData(
    const QStringList& row
    ) const
{
    return std::any_of(
        row.constBegin(),
        row.constEnd(),
        [](const QString& value)
        {
            return !value.trimmed().isEmpty();
        }
        );
}

void RosterModel::rebuildRows(
    const Roster& roster
    )
{
    m_rows.clear();

    for (int rowIndex = 0; rowIndex < RosterUi::RowCount; ++rowIndex)
    {
        QStringList row;
        row.reserve(m_columns.size());

        const QStringList sourceRow =
            rowIndex < roster.rows.size()
                ? roster.rows[rowIndex]
                : QStringList();

        for (int column = 0; column < m_columns.size(); ++column)
        {
            const int sourceColumn =
                findColumn(
                    m_columns[column],
                    roster.columns
                    );

            const QString value =
                sourceColumn >= 0 && sourceColumn < sourceRow.size()
                    ? sourceRow[sourceColumn]
                    : QString();

            row.append(
                normalizeCell(
                    value,
                    column
                    )
                );
        }

        m_rows.append(row);
    }
}

