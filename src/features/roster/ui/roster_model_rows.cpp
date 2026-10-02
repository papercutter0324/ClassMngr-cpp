#include "roster_model.h"

#include "core/utils/student_name_utils.h"
#include "ui/shared/qt_text_adapter.h"
#include "next/application/roster_row_transfer_preparation.h"
#include "next/application/roster_row_removal.h"
#include "next/application/roster_row_reordering.h"

#include <QCoreApplication>

#include <string>
#include <string_view>
#include <utility>
#include <variant>
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
    snapshot.columns.reserve(static_cast<std::size_t>(columns.size()));
    for (const QString& column : columns)
    {
        snapshot.columns.push_back(
            Ui::QtTextAdapter::toUtf16String(column)
            );
    }

    snapshot.rows.reserve(static_cast<std::size_t>(rows.size()));
    for (const QStringList& sourceRow : rows)
    {
        std::vector<std::u16string> row;
        row.reserve(static_cast<std::size_t>(sourceRow.size()));
        for (const QString& cell : sourceRow)
        {
            row.push_back(Ui::QtTextAdapter::toUtf16String(cell));
        }
        snapshot.rows.push_back(std::move(row));
    }

    return snapshot;
}

ClassMngr::Next::Application::RosterRowTransferPreparationResult
prepareTransferredRow(
    const QStringList& targetColumns,
    const QList<QStringList>& targetRows,
    const QStringList& sourceColumns,
    const QStringList& sourceRow
    )
{
    const auto snapshot = applicationSnapshot(targetColumns, targetRows);
    return ClassMngr::Next::Application::prepareRosterRowTransfer(
        snapshot.columns,
        snapshot.rows,
        toUtf16(sourceColumns),
        toUtf16(sourceRow),
        qtCaseInsensitiveEquals
        );
}

QString transferRejectionMessage(
    const ClassMngr::Next::Application::RosterRowTransferPreparationRejection
        rejection
    )
{
    using ClassMngr::Next::Application::RosterRowTransferPreparationRejection;
    switch (rejection)
    {
    case RosterRowTransferPreparationRejection::SourceRowHasNoData:
        return QCoreApplication::translate(
            "RosterModel",
            "Selected row is empty."
            );
    case RosterRowTransferPreparationRejection::TargetRosterIsFull:
        return QCoreApplication::translate(
            "RosterModel",
            "Target roster is full."
            );
    case RosterRowTransferPreparationRejection::DuplicateStudentNamePair:
        return QCoreApplication::translate(
            "RosterModel",
            "Target roster already contains this student."
            );
    }
    return {};
}

bool rejectTransfer(
    const ClassMngr::Next::Application::RosterRowTransferPreparationError& error,
    QString* reason
    )
{
    if (reason)
    {
        *reason = transferRejectionMessage(error.rejection);
    }
    return false;
}

QList<QStringList> qtRows(
    const std::vector<std::vector<std::u16string>>& rows
    )
{
    QList<QStringList> converted;
    converted.reserve(static_cast<qsizetype>(rows.size()));
    for (const std::vector<std::u16string>& sourceRow : rows)
    {
        QStringList row;
        row.reserve(static_cast<qsizetype>(sourceRow.size()));
        for (const std::u16string& cell : sourceRow)
        {
            row.append(Ui::QtTextAdapter::fromUtf16String(cell));
        }
        converted.append(std::move(row));
    }

    return converted;
}

} // namespace

bool RosterModel::canRemoveRow(
    int row,
    QString* reason
    ) const
{
    const auto result =
        ClassMngr::Next::Application::removeRosterRow(
            applicationSnapshot(m_columns, m_rows),
            row
            );
    const auto* error =
        std::get_if<
            ClassMngr::Next::Application::RosterRowRemovalError
            >(&result);
    if (!error)
    {
        return true;
    }

    if (reason)
    {
        using ClassMngr::Next::Application::RosterRowRemovalErrorCode;
        switch (error->code)
        {
        case RosterRowRemovalErrorCode::InvalidRowIndex:
            *reason = tr("Select a student row to remove.");
            break;
        case RosterRowRemovalErrorCode::RowHasNoData:
            *reason = tr("Selected row is already empty.");
            break;
        }
    }

    return false;
}

bool RosterModel::removeRosterRow(
    int row
    )
{
    auto result =
        ClassMngr::Next::Application::removeRosterRow(
            applicationSnapshot(m_columns, m_rows),
            row
            );
    auto* removed =
        std::get_if<ClassMngr::Next::Application::RosterSnapshot>(&result);
    if (!removed)
    {
        return false;
    }

    m_rows = qtRows(removed->rows);
    const int lastRow = m_rows.size() - 1;

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
    const auto result =
        ClassMngr::Next::Application::reorderRosterRows(
            applicationSnapshot(m_columns, m_rows),
            sourceRow,
            destinationRow
            );
    const auto* error =
        std::get_if<
            ClassMngr::Next::Application::RosterRowReorderingError
            >(&result);
    if (!error)
    {
        return true;
    }

    if (reason)
    {
        using ClassMngr::Next::Application::RosterRowReorderingErrorCode;
        switch (error->code)
        {
        case RosterRowReorderingErrorCode::InvalidSourceIndex:
            *reason = tr("Select a student row to move.");
            break;
        case RosterRowReorderingErrorCode::InvalidDestinationIndex:
            *reason = tr("Drop the student on another roster row.");
            break;
        case RosterRowReorderingErrorCode::SameRow:
            *reason = tr("Drop the student on a different row.");
            break;
        case RosterRowReorderingErrorCode::SourceRowHasNoData:
            *reason = tr("Selected row is empty.");
            break;
        }
    }

    return false;
}

bool RosterModel::moveRosterRow(
    int sourceRow,
    int destinationRow
    )
{
    auto result =
        ClassMngr::Next::Application::reorderRosterRows(
            applicationSnapshot(m_columns, m_rows),
            sourceRow,
            destinationRow
            );
    auto* reordered =
        std::get_if<ClassMngr::Next::Application::RosterSnapshot>(&result);
    if (!reordered)
    {
        return false;
    }

    m_rows = qtRows(reordered->rows);

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
        StudentNameUtils::namePairKey(
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
            && StudentNameUtils::namePairKey(
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
    const auto result =
        prepareTransferredRow(
            m_columns,
            m_rows,
            sourceColumns,
            sourceRow
            );
    const auto* error =
        std::get_if<
            ClassMngr::Next::Application::RosterRowTransferPreparationError
            >(&result);
    if (error)
    {
        return rejectTransfer(*error, reason);
    }

    return true;
}

bool RosterModel::insertTransferredRow(
    const QStringList& sourceColumns,
    const QStringList& sourceRow,
    QString* reason
    )
{
    auto result =
        prepareTransferredRow(
            m_columns,
            m_rows,
            sourceColumns,
            sourceRow
            );
    auto* prepared =
        std::get_if<
            ClassMngr::Next::Application::RosterRowTransferPreparation
            >(&result);
    if (!prepared)
    {
        return rejectTransfer(
            std::get<
                ClassMngr::Next::Application::RosterRowTransferPreparationError
                >(result),
            reason
            );
    }

    m_rows[static_cast<qsizetype>(prepared->destinationRow)] =
        qtStrings(prepared->mappedRow);

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
