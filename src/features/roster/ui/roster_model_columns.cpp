#include "roster_model.h"

#include "features/roster/ui/roster_constants.h"
#include "features/roster/ui/roster_qt_text_adapter.h"
#include "next/application/roster_custom_column_name_policy.h"
#include "next/application/roster_custom_column_removal_policy.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace
{

std::vector<std::u16string> toUtf16(
    const QStringList& values
    )
{
    std::vector<std::u16string> result;
    result.reserve(static_cast<std::size_t>(values.size()));
    for (const QString& value : values)
    {
        result.push_back(RosterUi::QtTextAdapter::toUtf16String(value));
    }
    return result;
}

bool qtCaseInsensitiveEquals(
    const std::u16string_view left,
    const std::u16string_view right
    )
{
    return RosterUi::QtTextAdapter::fromUtf16String(left).compare(
        RosterUi::QtTextAdapter::fromUtf16String(right),
        Qt::CaseInsensitive
        ) == 0;
}

} // namespace

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
    using ClassMngr::Next::Application::RosterCustomColumnNameRejection;
    const auto admission =
        ClassMngr::Next::Application::admitRosterCustomColumnName(
            RosterUi::QtTextAdapter::toUtf16String(name),
            toUtf16(m_columns),
            toUtf16(Roster::BaseColumns),
            qtCaseInsensitiveEquals
            );

    if (!admission.accepted())
    {
        if (reason && admission.rejection)
        {
            switch (*admission.rejection)
            {
            case RosterCustomColumnNameRejection::Empty:
                *reason = tr("Column name cannot be empty.");
                break;
            case RosterCustomColumnNameRejection::Duplicate:
                *reason = tr("A column with that name already exists.");
                break;
            case RosterCustomColumnNameRejection::RequiredColumn:
                *reason = tr("Required roster columns already exist.");
                break;
            }
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
    const ClassMngr::Next::Application::RosterSnapshot roster{
        .columns = toUtf16(m_columns)
    };
    const auto eligibility =
        ClassMngr::Next::Application::canRemoveRosterCustomColumn(
            roster,
            column,
            toUtf16(Roster::BaseColumns),
            qtCaseInsensitiveEquals
            );
    if (!eligibility.accepted())
    {
        if (reason && eligibility.rejection)
        {
            using ClassMngr::Next::Application::
                RosterCustomColumnRemovalRejection;
            switch (*eligibility.rejection)
            {
            case RosterCustomColumnRemovalRejection::InvalidColumnIndex:
                *reason = tr("Select a custom column to remove.");
                break;
            case RosterCustomColumnRemovalRejection::RequiredColumn:
                *reason = tr("Required roster columns cannot be removed.");
                break;
            }
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
    return RosterUi::QtTextAdapter::fromUtf16String(
        ClassMngr::Next::Application::normalizeRosterCustomColumnName(
            RosterUi::QtTextAdapter::toUtf16String(name),
            qtCaseInsensitiveEquals
            )
        );
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

