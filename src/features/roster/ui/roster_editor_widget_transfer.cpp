#include "roster_editor_widget.h"
#include "ui/shared/pages/autosave_coordinator.h"

#include "core/application_services.h"
#include "core/utils/sidebar_node_naming.h"
#include "data/data_service.h"
#include "features/roster/ui/roster_model.h"
#include "features/roster/ui/roster_table_view.h"

#include <QAction>
#include <QHash>
#include <QMenu>
#include <QMessageBox>
#include <QPair>
#include <QTimer>

#include <algorithm>
#include <utility>

namespace
{

struct TransferClassTarget
{
    int classId = -1;
    QString label;
    bool full = false;
};

QString sidebarClassDisplayName(
    DataService* dataService,
    int classId
    )
{
    if (!dataService || !dataService->isOpen() || classId <= 0)
    {
        return {};
    }

    const ClassInfo classInfo = dataService->loadClassInfo(classId);
    Teacher teacher;
    if (classInfo.teacherId > 0)
    {
        teacher = dataService->getTeacher(classInfo.teacherId);
    }

    return SidebarNodeNaming::formatClassDisplayName(classInfo, teacher);
}

} // namespace

void RosterEditorWidget::showRosterContextMenu(
    const QPoint& position
    )
{
    if (!m_table || !m_model)
    {
        return;
    }

    const QModelIndex clicked = m_table->indexAt(position);
    if (!clicked.isValid())
    {
        return;
    }

    m_table->setCurrentIndex(clicked);
    QString reason;
    const bool canRemove = m_model->canRemoveRow(clicked.row(), &reason);

    QMenu menu(this);
    QAction* removeAction = menu.addAction(tr("Remove Student"));
    removeAction->setEnabled(canRemove);
    if (!canRemove && !reason.isEmpty())
    {
        removeAction->setToolTip(reason);
    }

    QMenu* transferMenu =
        m_testingClassMode
            ? nullptr
            : menu.addMenu(tr("Transfer Class"));
    if (transferMenu)
    {
        transferMenu->setEnabled(canRemove);
    }
    QHash<QAction*, int> transferActions;
    auto* dataService = m_services ? m_services->dataService() : nullptr;

    const QString currentGrade =
        dataService && dataService->isOpen() && m_classroom.id > 0
            ? dataService->loadClassInfo(m_classroom.id).classGrade.trimmed()
            : QString();
    QList<TransferClassTarget> targets;

    if (canRemove && dataService && dataService->isOpen() && !currentGrade.isEmpty())
    {
        for (const Classroom& classroom : dataService->getClasses())
        {
            if (classroom.id <= 0 || classroom.id == m_classroom.id)
            {
                continue;
            }

            const ClassInfo targetInfo = dataService->loadClassInfo(classroom.id);
            if (targetInfo.classGrade.trimmed() != currentGrade)
            {
                continue;
            }

            RosterModel targetModel;
            targetModel.setRoster(dataService->loadRoster(classroom.id));
            TransferClassTarget target;
            target.classId = classroom.id;
            target.label = sidebarClassDisplayName(dataService, classroom.id);
            if (target.label.trimmed().isEmpty())
            {
                target.label = classroom.name.trimmed().isEmpty()
                    ? tr("Class %1").arg(classroom.id)
                    : classroom.name.trimmed();
            }
            target.full = targetModel.firstEmptyRow() < 0;
            targets.append(target);
        }

        std::sort(
            targets.begin(),
            targets.end(),
            [](const TransferClassTarget& left, const TransferClassTarget& right)
            {
                const int comparison = QString::localeAwareCompare(left.label, right.label);
                return comparison != 0
                    ? comparison < 0
                    : left.classId < right.classId;
            }
            );
    }

    if (transferMenu && targets.isEmpty())
    {
        QAction* emptyAction = transferMenu->addAction(tr("No same-grade classes"));
        emptyAction->setEnabled(false);
    }
    else if (transferMenu)
    {
        for (const TransferClassTarget& target : std::as_const(targets))
        {
            QAction* transferAction = transferMenu->addAction(
                target.full ? tr("%1 (full)").arg(target.label) : target.label
                );
            transferAction->setEnabled(!target.full);
            if (target.full)
            {
                transferAction->setToolTip(tr("Target roster is full."));
            }
            else
            {
                transferActions.insert(transferAction, target.classId);
            }
        }
    }

    QAction* selectedAction = menu.exec(m_table->viewport()->mapToGlobal(position));
    if (selectedAction == removeAction && canRemove)
    {
        removeRosterRow(clicked.row());
    }
    else if (transferActions.contains(selectedAction))
    {
        transferRosterRow(clicked.row(), transferActions.value(selectedAction));
    }
}

void RosterEditorWidget::transferRosterRow(
    int row,
    int targetClassId
    )
{
    if (
        !m_model
        || !m_services
        || !m_services->dataService()
        || !m_services->dataService()->isOpen()
        || m_classroom.id <= 0
        || targetClassId <= 0
        || targetClassId == m_classroom.id
        )
    {
        return;
    }

    QString reason;
    if (!m_model->canRemoveRow(row, &reason))
    {
        QMessageBox::warning(this, tr("Cannot Transfer Student"), reason);
        return;
    }
    if (!validateRosterBeforeSave(true))
    {
        return;
    }

    auto* dataService = m_services->dataService();
    const QStringList sourceColumns = m_model->columnNames();
    const QStringList sourceRow = m_model->rowValues(row);
    const Roster targetSourceRoster = dataService->loadRoster(targetClassId);
    RosterModel targetModel;
    targetModel.setRoster(targetSourceRoster);

    reason.clear();
    if (!targetModel.insertTransferredRow(sourceColumns, sourceRow, &reason))
    {
        QMessageBox::warning(
            this,
            tr("Cannot Transfer Student"),
            reason.isEmpty()
                ? tr("The student could not be transferred.")
                : reason
            );
        return;
    }

    Roster targetRoster = targetModel.toRoster();
    targetRoster.columnWidths = normalizedColumnWidths(
        targetSourceRoster,
        targetRoster.columns
        );
    const Roster sourceRoster = rosterWithRowRemoved(row);
    const bool saved = dataService->saveRosters(
        {
            qMakePair(m_classroom.id, sourceRoster),
            qMakePair(targetClassId, targetRoster)
        }
        );
    if (!saved)
    {
        QMessageBox::warning(
            this,
            tr("Cannot Transfer Student"),
            tr("The roster changes could not be saved.")
            );
        return;
    }

    m_autosave->cancelPendingSave();

    m_removingRosterRow = true;
    m_model->removeRosterRow(row);
    m_removingRosterRow = false;
    m_model->clearDirty();
    m_widthsDirty = false;
    m_autosave->markClean();

    const int nextRow =
        row < m_model->rowCount()
            ? row
            : m_model->rowCount() - 1;
    if (nextRow >= 0 && m_model->columnCount() > 0)
    {
        selectRosterCell(nextRow, 0);
    }

    updateActions();
}

Roster RosterEditorWidget::rosterWithRowRemoved(
    int row
    ) const
{
    Roster roster = currentRosterForSave();
    if (row < 0 || row >= roster.rows.size())
    {
        return roster;
    }

    const int lastRow = roster.rows.size() - 1;
    for (int sourceRow = row + 1; sourceRow <= lastRow; ++sourceRow)
    {
        roster.rows[sourceRow - 1] = roster.rows[sourceRow];
    }
    roster.rows[lastRow] = QStringList(roster.columns.size(), QString());
    return roster;
}
