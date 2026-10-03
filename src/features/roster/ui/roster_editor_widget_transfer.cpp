#include "roster_editor_widget.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/autosave_coordinator.h"

#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "core/utils/sidebar_node_naming.h"
#include "features/roster/ui/roster_model.h"
#include "features/roster/ui/roster_table_view.h"
#include "next/application/classes_list_read_query.h"
#include "next/application/roster_read_query.h"
#include "next/application/roster_transfer_target_eligibility.h"
#include "next/application/selected_class_subtitle_read_query.h"
#include "next/domain/domain_types.h"
#include "next/platform/application_services_classes_list_read_port.h"
#include "next/platform/application_services_roster_read_port.h"
#include "next/platform/application_services_selected_class_subtitle_read_port.h"
#include "ui/shared/qt_text_adapter.h"

#include <QAction>
#include <QHash>
#include <QMenu>
#include <QPair>
#include <QTimer>

#include <algorithm>
#include <charconv>
#include <string>
#include <system_error>
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

QList<Classroom> classroomsFromListSnapshot(
    const ClassMngr::Next::Application::ClassesListSnapshot& snapshot
    )
{
    QList<Classroom> classrooms;
    classrooms.reserve(static_cast<qsizetype>(snapshot.classes.size()));
    for (const auto& entry : snapshot.classes)
    {
        int classId = 0;
        const std::string& classIdValue = entry.classId.value();
        const auto [end, error] = std::from_chars(
            classIdValue.data(),
            classIdValue.data() + classIdValue.size(),
            classId
            );
        Q_ASSERT(
            error == std::errc{}
            && end == classIdValue.data() + classIdValue.size()
            && classId > 0
            );
        if (error != std::errc{}
            || end != classIdValue.data() + classIdValue.size()
            || classId <= 0)
        {
            continue;
        }

        Classroom classroom;
        classroom.id = classId;
        classroom.name = QString::fromStdU16String(entry.className);
        classrooms.append(std::move(classroom));
    }
    return classrooms;
}

QString classesListErrorMessage(const std::string& message)
{
    return QString::fromUtf8(
        message.data(),
        static_cast<qsizetype>(message.size())
        );
}

Roster rosterFromSnapshot(
    const ClassMngr::Next::Application::RosterSnapshot& snapshot
    )
{
    Roster roster;
    roster.columns.reserve(static_cast<qsizetype>(snapshot.columns.size()));
    for (const auto& column : snapshot.columns)
    {
        roster.columns.append(QString::fromStdU16String(column));
    }

    roster.columnWidths.reserve(
        static_cast<qsizetype>(snapshot.columnWidths.size())
        );
    for (const int width : snapshot.columnWidths)
    {
        roster.columnWidths.append(width);
    }

    roster.rows.reserve(static_cast<qsizetype>(snapshot.rows.size()));
    for (const auto& snapshotRow : snapshot.rows)
    {
        QStringList row;
        row.reserve(static_cast<qsizetype>(snapshotRow.size()));
        for (const auto& cell : snapshotRow)
        {
            row.append(QString::fromStdU16String(cell));
        }
        roster.rows.append(std::move(row));
    }

    return roster;
}

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
        ClassMngr::Next::Platform::
            ApplicationServicesClassesListReadPort readPort(m_services);
        const ClassMngr::Next::Application::ClassesListReadQuery query(
            readPort
            );
        const auto classes = query.execute();
        QList<Classroom> availableClasses;
        if (!classes)
        {
            DialogServices::showWarning(
                this,
                tr("Transfer Student"),
                tr("Transfer classes could not be loaded."),
                classesListErrorMessage(classes.error().message)
                );
            transferMenu->setEnabled(false);
        }
        else
        {
            availableClasses = classroomsFromListSnapshot(classes.value());
        }
        for (const Classroom& classroom : availableClasses)
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

            Roster targetRoster;
            const auto typedClassId =
                ClassMngr::Next::Domain::ClassId::fromString(
                    std::to_string(classroom.id)
                    );
            if (typedClassId)
            {
                ClassMngr::Next::Platform::
                    ApplicationServicesRosterReadPort readPort(m_services);
                const ClassMngr::Next::Application::RosterReadQuery query{
                    .classId = *typedClassId
                };
                const auto loadedRoster =
                    ClassMngr::Next::Application::RosterReadUseCase::execute(
                        query,
                        readPort
                        );
                if (loadedRoster)
                {
                    const auto& snapshot = loadedRoster.value();
                    for (const auto& column : snapshot.columns)
                    {
                        targetRoster.columns.append(
                            QString::fromStdU16String(column)
                            );
                    }

                    for (const auto& snapshotRow : snapshot.rows)
                    {
                        QStringList row;
                        row.reserve(
                            static_cast<qsizetype>(snapshotRow.size())
                            );
                        for (const auto& cell : snapshotRow)
                        {
                            row.append(QString::fromStdU16String(cell));
                        }
                        targetRoster.rows.append(std::move(row));
                    }
                }
            }

            RosterModel targetModel;
            targetModel.setRoster(targetRoster);
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
    Roster targetSourceRoster;
    const auto typedTargetClassId =
        ClassMngr::Next::Domain::ClassId::fromString(
            std::to_string(targetClassId)
            );
    if (typedTargetClassId)
    {
        ClassMngr::Next::Platform::
            ApplicationServicesRosterReadPort readPort(m_services);
        const ClassMngr::Next::Application::RosterReadQuery query{
            .classId = *typedTargetClassId
        };
        const auto loadedRoster =
            ClassMngr::Next::Application::RosterReadUseCase::execute(
                query,
                readPort
                );
        if (loadedRoster)
        {
            targetSourceRoster = rosterFromSnapshot(loadedRoster.value());
        }
    }
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
