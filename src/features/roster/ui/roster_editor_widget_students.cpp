#include "roster_editor_widget.h"

#include "features/roster/ui/roster_model.h"
#include "features/roster/ui/roster_table_view.h"

#include <QAbstractButton>
#include <QMessageBox>
#include <QPushButton>

void RosterEditorWidget::moveStudentRow(
    int sourceRow,
    int destinationRow
    )
{
    if (!m_model || !m_table)
    {
        return;
    }

    QString reason;
    if (!m_model->canMoveRow(sourceRow, destinationRow, &reason))
    {
        Q_UNUSED(reason);
        return;
    }

    const int currentColumn =
        m_table->currentIndex().isValid()
            ? m_table->currentIndex().column()
            : 0;

    m_movingRosterRow = true;
    const bool moved = m_model->moveRosterRow(sourceRow, destinationRow);
    m_movingRosterRow = false;

    if (!moved)
    {
        return;
    }

    selectRosterCell(
        destinationRow,
        qBound(0, currentColumn, m_model->columnCount() - 1)
        );
    scheduleAutosave();
    updateActions();
}

void RosterEditorWidget::handleNameCellChanged(
    const QModelIndex& topLeft,
    const QModelIndex& bottomRight
    )
{
    if (
        m_loadingRoster
        || m_resolvingDuplicateName
        || m_removingRosterRow
        || m_movingRosterRow
        || !m_model
        || !topLeft.isValid()
        || !bottomRight.isValid()
        )
    {
        return;
    }

    m_table->viewport()->update();

    for (int row = topLeft.row(); row <= bottomRight.row(); ++row)
    {
        for (int column = topLeft.column(); column <= bottomRight.column(); ++column)
        {
            if (m_model->isNameColumn(column))
            {
                resolveDuplicateName(row, column);
                return;
            }
        }
    }
}

void RosterEditorWidget::resolveDuplicateName(
    int row,
    int editedColumn
    )
{
    if (!m_model || !m_table || !m_model->isNameColumn(editedColumn))
    {
        return;
    }

    const QList<int> duplicateRows = m_model->duplicateNameRows(row);
    if (duplicateRows.isEmpty())
    {
        return;
    }

    QStringList duplicateRowLabels;
    for (int duplicateRow : duplicateRows)
    {
        duplicateRowLabels.append(QString::number(duplicateRow + 1));
    }

    const int koreanColumn = m_model->koreanNameColumn();
    const QString suggestedName = m_model->suggestedKoreanNameWithSuffix(row);

    QMessageBox dialog(this);
    dialog.setIcon(QMessageBox::Warning);
    dialog.setWindowTitle(tr("Duplicate Student Name"));
    dialog.setText(tr("This English/Korean name combination already exists."));
    dialog.setInformativeText(
        tr("Duplicate row(s): %1").arg(duplicateRowLabels.join(QStringLiteral(", ")))
        );

    QPushButton* suffixButton = dialog.addButton(
        suggestedName.isEmpty()
            ? tr("No Suffix Available")
            : tr("Use %1").arg(suggestedName),
        QMessageBox::AcceptRole
        );
    suffixButton->setEnabled(!suggestedName.isEmpty());
    QPushButton* clearButton = dialog.addButton(
        tr("Clear Edited Cell"),
        QMessageBox::DestructiveRole
        );
    QPushButton* locateButton = dialog.addButton(
        tr("Locate Duplicate"),
        QMessageBox::ActionRole
        );
    QPushButton* keepButton = dialog.addButton(
        tr("Keep As-Is"),
        QMessageBox::RejectRole
        );

    dialog.setDefaultButton(
        suggestedName.isEmpty() ? clearButton : suffixButton
        );
    dialog.exec();

    QAbstractButton* clickedButton = dialog.clickedButton();
    if (clickedButton == suffixButton && !suggestedName.isEmpty())
    {
        m_resolvingDuplicateName = true;
        m_model->setData(
            m_model->index(row, koreanColumn),
            suggestedName,
            Qt::EditRole
            );
        m_resolvingDuplicateName = false;
        selectRosterCell(row, koreanColumn);
    }
    else if (clickedButton == clearButton)
    {
        m_resolvingDuplicateName = true;
        m_model->setData(
            m_model->index(row, editedColumn),
            QString(),
            Qt::EditRole
            );
        m_resolvingDuplicateName = false;
        selectRosterCell(row, editedColumn);
    }
    else if (clickedButton == locateButton)
    {
        selectRosterCell(duplicateRows.first(), m_model->englishNameColumn());
    }
    else
    {
        Q_UNUSED(keepButton);
    }

    m_table->viewport()->update();
    updateActions();
}

void RosterEditorWidget::selectRosterCell(
    int row,
    int column
    )
{
    if (!m_model || !m_table)
    {
        return;
    }

    const QModelIndex index = m_model->index(row, column);
    if (!index.isValid())
    {
        return;
    }

    m_table->setCurrentIndex(index);
    m_table->scrollTo(index);
}

bool RosterEditorWidget::removeRosterRow(
    int row
    )
{
    if (!m_model || !m_table)
    {
        return false;
    }

    QString reason;
    if (!m_model->canRemoveRow(row, &reason))
    {
        QMessageBox::warning(this, tr("Cannot Remove Student"), reason);
        return false;
    }

    const QString label = rosterRowLabel(row);
    const QString message =
        label.isEmpty()
            ? tr("Remove row %1 from the roster?").arg(row + 1)
            : tr("Remove %1 from the roster?").arg(label);

    if (QMessageBox::question(this, tr("Remove Student"), message) != QMessageBox::Yes)
    {
        return false;
    }

    m_removingRosterRow = true;
    const bool removed = m_model->removeRosterRow(row);
    m_removingRosterRow = false;

    if (!removed)
    {
        return false;
    }

    const int nextRow =
        row < m_model->rowCount()
            ? row
            : m_model->rowCount() - 1;

    if (nextRow >= 0 && m_model->columnCount() > 0)
    {
        selectRosterCell(nextRow, 0);
    }

    scheduleAutosave();
    updateActions();
    return true;
}

QString RosterEditorWidget::rosterRowLabel(
    int row
    ) const
{
    if (!m_model || row < 0 || row >= m_model->rowCount())
    {
        return {};
    }

    QStringList names;
    const int englishColumn = m_model->englishNameColumn();
    if (englishColumn >= 0)
    {
        const QString englishName = m_model->index(row, englishColumn)
                                        .data(Qt::DisplayRole)
                                        .toString()
                                        .trimmed();
        if (!englishName.isEmpty())
        {
            names.append(englishName);
        }
    }

    const int koreanColumn = m_model->koreanNameColumn();
    if (koreanColumn >= 0)
    {
        const QString koreanName = m_model->index(row, koreanColumn)
                                       .data(Qt::DisplayRole)
                                       .toString()
                                       .trimmed();
        if (!koreanName.isEmpty())
        {
            names.append(koreanName);
        }
    }

    return names.join(QStringLiteral(" / "));
}
