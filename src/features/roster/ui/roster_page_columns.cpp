#include "roster_page.h"

#include "features/roster/ui/roster_column_layout_controller.h"
#include "features/roster/ui/roster_model.h"
#include "features/roster/ui/roster_table_view.h"

#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>

void RosterPage::addColumn()
{
    bool accepted = false;
    const QString name = QInputDialog::getText(
        this,
        tr("Add Column"),
        tr("Column name:"),
        QLineEdit::Normal,
        QString(),
        &accepted
        ).trimmed();

    if (!accepted)
    {
        return;
    }

    QString reason;
    if (!m_model->canAddColumn(name, &reason))
    {
        QMessageBox::warning(this, tr("Cannot Add Column"), reason);
        return;
    }

    if (!m_model->insertCustomColumn(name))
    {
        return;
    }

    const int column = m_model->columnCount() - 1;
    m_layoutController->applyResizeModes();
    m_layoutController->initializeAddedCustomColumn(column);

    const QModelIndex firstCell = m_model->index(0, column);
    m_table->setCurrentIndex(firstCell);
    m_table->edit(firstCell);
    scheduleAutosave();
    updateActions();
}

void RosterPage::removeColumn()
{
    const QModelIndex current = m_table->currentIndex();
    QString reason;

    if (!m_model->canRemoveColumn(current.column(), &reason))
    {
        QMessageBox::warning(this, tr("Cannot Remove Column"), reason);
        return;
    }

    const QString columnName = m_model->columnName(current.column());
    const QMessageBox::StandardButton response = QMessageBox::question(
        this,
        tr("Remove Column"),
        tr("Remove the \"%1\" column?").arg(columnName)
        );

    if (response != QMessageBox::Yes)
    {
        return;
    }

    const int removedWidth = m_table->columnWidth(current.column());
    m_model->removeRosterColumn(current.column());
    m_layoutController->applyResizeModes();
    m_layoutController->handleCustomColumnRemoved(removedWidth);
    scheduleAutosave();
    updateActions();
}
