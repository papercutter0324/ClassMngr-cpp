#include "roster_model.h"

#include "features/roster/ui/roster_constants.h"
#include "ui/shared/qt_text_adapter.h"
#include "next/application/roster_row_availability.h"
#include "next/application/roster_custom_column_append.h"
#include "next/application/roster_custom_column_name_policy.h"
#include "next/application/roster_custom_column_removal_policy.h"

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
        result.push_back(Ui::QtTextAdapter::toUtf16String(value));
    }
    return result;
}

bool qtCaseInsensitiveEquals(
    const std::u16string_view left,
    const std::u16string_view right
    )
{
    return Ui::QtTextAdapter::fromUtf16String(left).compare(
        Ui::QtTextAdapter::fromUtf16String(right),
        Qt::CaseInsensitive
        ) == 0;
}

ClassMngr::Next::Application::RosterSnapshot applicationSnapshot(
    const QStringList& columns,
    const QList<QStringList>& rows
    )
{
    ClassMngr::Next::Application::RosterSnapshot snapshot;
    snapshot.columns = toUtf16(columns);
    snapshot.rows.reserve(static_cast<std::size_t>(rows.size()));
    for (const QStringList& sourceRow : rows)
    {
        snapshot.rows.push_back(toUtf16(sourceRow));
    }
    return snapshot;
}

QStringList qtStrings(
    const std::vector<std::u16string>& values
    )
{
    QStringList result;
    result.reserve(static_cast<qsizetype>(values.size()));
    for (const std::u16string& value : values)
    {
        result.append(Ui::QtTextAdapter::fromUtf16String(value));
    }
    return result;
}

QList<QStringList> qtRows(
    const std::vector<std::vector<std::u16string>>& rows
    )
{
    QList<QStringList> result;
    result.reserve(static_cast<qsizetype>(rows.size()));
    for (const std::vector<std::u16string>& sourceRow : rows)
    {
        result.append(qtStrings(sourceRow));
    }
    return result;
}

std::vector<std::vector<std::u16string>> applicationRows(
    const QList<QStringList>& rows
    )
{
    std::vector<std::vector<std::u16string>> result;
    result.reserve(static_cast<std::size_t>(rows.size()));
    for (const QStringList& sourceRow : rows)
    {
        result.push_back(toUtf16(sourceRow));
    }
    return result;
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
    const auto rows = applicationRows(m_rows);
    const std::size_t firstEmpty =
        ClassMngr::Next::Application::firstEmptyRosterRow(rows);
    return firstEmpty < rows.size()
        ? static_cast<int>(firstEmpty)
        : -1;
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
            Ui::QtTextAdapter::toUtf16String(name),
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
    const auto result =
        ClassMngr::Next::Application::appendRosterCustomColumn(
            applicationSnapshot(m_columns, m_rows),
            Ui::QtTextAdapter::toUtf16String(name),
            toUtf16(Roster::BaseColumns),
            qtCaseInsensitiveEquals
            );
    const auto* appended =
        std::get_if<ClassMngr::Next::Application::RosterSnapshot>(&result);
    if (!appended)
    {
        return false;
    }

    const int column =
        m_columns.size();

    beginInsertColumns(
        QModelIndex(),
        column,
        column
        );

    m_columns = qtStrings(appended->columns);
    m_rows = qtRows(appended->rows);

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
    return Ui::QtTextAdapter::fromUtf16String(
        ClassMngr::Next::Application::normalizeRosterCustomColumnName(
            Ui::QtTextAdapter::toUtf16String(name),
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

void RosterModel::rebuildRows(
    const Roster& roster,
    const ClassMngr::Next::Application::RosterColumnProjection& projection
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
            const auto& projectedColumn = projection.columns().at(
                static_cast<std::size_t>(column)
                );

            const QString value =
                projectedColumn.sourceColumnIndex
                    && *projectedColumn.sourceColumnIndex
                        < static_cast<std::size_t>(sourceRow.size())
                    ? sourceRow[static_cast<qsizetype>(
                        *projectedColumn.sourceColumnIndex
                        )]
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

