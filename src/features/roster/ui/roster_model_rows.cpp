#include "roster_model.h"

#include <algorithm>

bool RosterModel::canRemoveRow(
    int row,
    QString* reason
    ) const
{
    if (row < 0 || row >= m_rows.size())
    {
        if (reason)
        {
            *reason = tr("Select a student row to remove.");
        }

        return false;
    }

    const bool hasData =
        std::any_of(
            m_rows[row].constBegin(),
            m_rows[row].constEnd(),
            [](const QString& value)
            {
                return !value.trimmed().isEmpty();
            }
            );

    if (!hasData)
    {
        if (reason)
        {
            *reason = tr("Selected row is already empty.");
        }

        return false;
    }

    return true;
}

bool RosterModel::removeRosterRow(
    int row
    )
{
    QString reason;

    if (!canRemoveRow(row, &reason))
    {
        Q_UNUSED(reason);
        return false;
    }

    const int lastRow =
        m_rows.size() - 1;

    for (int sourceRow = row + 1; sourceRow <= lastRow; ++sourceRow)
    {
        m_rows[sourceRow - 1] =
            m_rows[sourceRow];
    }

    m_rows[lastRow] =
        QStringList(
            m_columns.size(),
            QString()
            );

    validateAll();

    if (!m_columns.isEmpty())
    {
        emit dataChanged(
            index(
                0,
                0
                ),
            index(
                lastRow,
                m_columns.size() - 1
                ),
            {
                Qt::DisplayRole,
                Qt::EditRole,
                Qt::ToolTipRole
            }
            );
    }

    setDirty(true);

    return true;
}

bool RosterModel::canMoveRow(
    int sourceRow,
    int destinationRow,
    QString* reason
    ) const
{
    if (sourceRow < 0 || sourceRow >= m_rows.size())
    {
        if (reason)
        {
            *reason = tr("Select a student row to move.");
        }

        return false;
    }

    if (destinationRow < 0 || destinationRow >= m_rows.size())
    {
        if (reason)
        {
            *reason = tr("Drop the student on another roster row.");
        }

        return false;
    }

    if (sourceRow == destinationRow)
    {
        if (reason)
        {
            *reason = tr("Drop the student on a different row.");
        }

        return false;
    }

    const bool hasData =
        std::any_of(
            m_rows[sourceRow].constBegin(),
            m_rows[sourceRow].constEnd(),
            [](const QString& value)
            {
                return !value.trimmed().isEmpty();
            }
            );

    if (!hasData)
    {
        if (reason)
        {
            *reason = tr("Selected row is empty.");
        }

        return false;
    }

    return true;
}

bool RosterModel::moveRosterRow(
    int sourceRow,
    int destinationRow
    )
{
    QString reason;

    if (!canMoveRow(sourceRow, destinationRow, &reason))
    {
        Q_UNUSED(reason);
        return false;
    }

    const QStringList movedRow =
        m_rows.takeAt(sourceRow);

    m_rows.insert(
        destinationRow,
        movedRow
        );

    validateAll();

    if (!m_columns.isEmpty())
    {
        emit dataChanged(
            index(
                0,
                0
                ),
            index(
                m_rows.size() - 1,
                m_columns.size() - 1
                ),
            {
                Qt::DisplayRole,
                Qt::EditRole,
                Qt::ToolTipRole
            }
            );
    }

    setDirty(true);

    return true;
}

bool RosterModel::hasDuplicateTransferredStudent(
    const QStringList& sourceColumns,
    const QStringList& sourceRow,
    QString* reason
    ) const
{
    const QStringList mappedRow =
        mappedTransferRow(
            sourceColumns,
            sourceRow
            );

    const int englishColumn =
        englishNameColumn();

    const int koreanColumn =
        koreanNameColumn();

    if (
        englishColumn < 0
        || koreanColumn < 0
        || englishColumn >= mappedRow.size()
        || koreanColumn >= mappedRow.size()
        )
    {
        return false;
    }

    const QString key =
        namePairKey(
            mappedRow[englishColumn],
            mappedRow[koreanColumn]
            );

    if (key.isEmpty())
    {
        return false;
    }

    for (const QStringList& existingRow : m_rows)
    {
        if (
            englishColumn < existingRow.size()
            && koreanColumn < existingRow.size()
            && namePairKey(
                existingRow[englishColumn],
                existingRow[koreanColumn]
                ) == key
            )
        {
            if (reason)
            {
                *reason =
                    tr("Target roster already contains this student.");
            }

            return true;
        }
    }

    return false;
}

bool RosterModel::canInsertTransferredRow(
    const QStringList& sourceColumns,
    const QStringList& sourceRow,
    QString* reason
    ) const
{
    const QStringList mappedRow =
        mappedTransferRow(
            sourceColumns,
            sourceRow
            );

    if (!rowHasData(mappedRow))
    {
        if (reason)
        {
            *reason = tr("Selected row is empty.");
        }

        return false;
    }

    if (firstEmptyRow() < 0)
    {
        if (reason)
        {
            *reason = tr("Target roster is full.");
        }

        return false;
    }

    if (
        hasDuplicateTransferredStudent(
            sourceColumns,
            sourceRow,
            reason
            )
        )
    {
        return false;
    }

    return true;
}

bool RosterModel::insertTransferredRow(
    const QStringList& sourceColumns,
    const QStringList& sourceRow,
    QString* reason
    )
{
    if (
        !canInsertTransferredRow(
            sourceColumns,
            sourceRow,
            reason
            )
        )
    {
        return false;
    }

    const int destinationRow =
        firstEmptyRow();

    if (destinationRow < 0)
    {
        return false;
    }

    m_rows[destinationRow] =
        mappedTransferRow(
            sourceColumns,
            sourceRow
            );

    validateAll();

    if (!m_columns.isEmpty())
    {
        emit dataChanged(
            index(
                0,
                0
                ),
            index(
                m_rows.size() - 1,
                m_columns.size() - 1
                ),
            {
                Qt::DisplayRole,
                Qt::EditRole,
                Qt::ToolTipRole
            }
            );
    }

    setDirty(true);

    return true;
}
