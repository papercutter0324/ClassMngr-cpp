#include "roster_editor_widget.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/autosave_coordinator.h"

#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "core/utils/sidebar_node_naming.h"
#include "features/roster/ui/roster_model.h"
#include "features/roster/ui/roster_table_view.h"
#include "next/application/roster_transfer_target_eligibility.h"
#include "next/application/selected_class_subtitle_read_query.h"
#include "next/domain/domain_types.h"
#include "next/platform/application_services_selected_class_subtitle_read_port.h"
#include "ui/shared/qt_text_adapter.h"

#include <QAction>
#include <QHash>
#include <QMenu>
#include <QPair>
#include <QTimer>

#include <algorithm>
#include <string>
#include <utility>

namespace
{

struct TransferClassTarget
{
    int classId = -1;
    QString label;
    bool full = false;
};

struct TransferClassMetadata
{
    QString grade;
    QString displayName;
};

TransferClassMetadata readTransferClassMetadata(
    ApplicationServices* services,
    int classId
    )
{
    if (!services || classId <= 0)
    {
        return {};
    }

    const auto selectedClassId =
        ClassMngr::Next::Domain::ClassId::fromString(
            std::to_string(classId)
            );
    if (!selectedClassId)
    {
        return {};
    }

    ClassMngr::Next::Platform::
        ApplicationServicesSelectedClassSubtitleReadPort readPort(services);
    const ClassMngr::Next::Application::SelectedClassSubtitleReadQuery query(
        readPort
        );
    const auto loadedSubtitle = query.execute(*selectedClassId);
    if (!loadedSubtitle || !loadedSubtitle.value().classFields)
    {
        return {};
    }

    const auto& subtitle = loadedSubtitle.value();
    const auto& fields = subtitle.classFields.value();
    ClassInfo classInfo;
    classInfo.classGrade = QString::fromStdU16String(fields.classGrade);
    classInfo.classLevel = QString::fromStdU16String(fields.classLevel);
    classInfo.classTimes.reserve(
        static_cast<qsizetype>(fields.regularSchedule.size())
        );
    for (const auto& row : fields.regularSchedule)
    {
        ClassTime time;
        time.day = QString::fromStdU16String(row.day);
        time.startTime = QString::fromStdU16String(row.startTime);
        time.endTime.clear();
        classInfo.classTimes.append(std::move(time));
    }

    Teacher teacher;
    if (subtitle.assignedTeacher && subtitle.assignedTeacher.value())
    {
        const auto& teacherFields = subtitle.assignedTeacher.value().value();
        teacher.teacherKr = QString::fromStdU16String(
            teacherFields.teacherKr
            );
        teacher.teacherEn = QString::fromStdU16String(
            teacherFields.teacherEn
            );
        teacher.preferredRomanization = QString::fromStdU16String(
            teacherFields.preferredRomanization
            );
        teacher.preferredName = QString::fromStdU16String(
            teacherFields.preferredName
            );
    }

    return {
        classInfo.classGrade,
        SidebarNodeNaming::formatClassDisplayName(classInfo, teacher)
    };
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
    auto* classService = m_services ? m_services->classService() : nullptr;
    auto* rosterService = m_services ? m_services->rosterService() : nullptr;

    const TransferClassMetadata sourceMetadata =
        classService
            && ClassMngr::Next::Application::hasValidRosterTransferSourceId(
                m_classroom.id
                )
            ? readTransferClassMetadata(m_services, m_classroom.id)
            : TransferClassMetadata{};
    const QString currentGrade = sourceMetadata.grade;
    QList<TransferClassTarget> targets;

    if (canRemove
        && classService
        && rosterService
        && ClassMngr::Next::Application::isRosterTransferSourceEligible(
            m_classroom.id,
            Ui::QtTextAdapter::toUtf16String(currentGrade)
            ))
    {
        const Result<QList<Classroom>> classes = classService->classes();
        if (!classes)
        {
            DialogServices::showWarning(
                this,
                tr("Transfer Student"),
                tr("Transfer classes could not be loaded."),
                classes.error()
                );
            transferMenu->setEnabled(false);
        }
        for (const Classroom& classroom : classes.value_or(
                 QList<Classroom>{}))
        {
            if (!ClassMngr::Next::Application::shouldReadRosterTransferTargetClassInfo(
                    m_classroom.id,
                    classroom.id
                    ))
            {
                continue;
            }

            const TransferClassMetadata targetMetadata =
                readTransferClassMetadata(m_services, classroom.id);
            if (!ClassMngr::Next::Application::isRosterTransferTargetEligible(
                    m_classroom.id,
                    Ui::QtTextAdapter::toUtf16String(currentGrade),
                    classroom.id,
                    Ui::QtTextAdapter::toUtf16String(targetMetadata.grade)
                    ))
            {
                continue;
            }

            RosterModel targetModel;
            targetModel.setRoster(
                rosterService->roster(classroom.id).value_or(Roster{})
                );
            TransferClassTarget target;
            target.classId = classroom.id;
            target.label = targetMetadata.displayName;
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
        || !m_services->rosterService()
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
        DialogServices::showWarning(this, tr("Cannot Transfer Student"), reason);
        return;
    }
    if (!validateRosterBeforeSave(true, true))
    {
        return;
    }

    auto* rosterService = m_services->rosterService();
    const QStringList sourceColumns = m_model->columnNames();
    const QStringList sourceRow = m_model->rowValues(row);
    const Roster targetSourceRoster =
        rosterService->roster(targetClassId).value_or(Roster{});
    RosterModel targetModel;
    targetModel.setRoster(targetSourceRoster);

    reason.clear();
    if (!targetModel.insertTransferredRow(sourceColumns, sourceRow, &reason))
    {
        DialogServices::showWarning(
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
    const Status saved = rosterService->saveRosters(
        {
            qMakePair(m_classroom.id, sourceRoster),
            qMakePair(targetClassId, targetRoster)
        }
        );
    if (!saved)
    {
        DialogServices::showWarning(
            this,
            tr("Cannot Transfer Student"),
            saved.error()
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
