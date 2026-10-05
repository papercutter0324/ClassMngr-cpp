#include "roster_editor_widget.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/autosave_coordinator.h"

#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "core/utils/sidebar_node_naming.h"
#include "features/roster/ui/roster_model.h"
#include "features/roster/ui/roster_table_view.h"
#include "next/application/classes_list_read_query.h"
#include "next/application/roster_availability_batch_read_query.h"
#include "next/application/roster_row_transfer_use_case.h"
#include "next/application/roster_transfer_target_eligibility.h"
#include "next/application/selected_class_subtitle_batch_read_query.h"
#include "next/application/selected_class_subtitle_read_query.h"
#include "next/domain/domain_types.h"
#include "next/platform/application_services_classes_list_read_port.h"
#include "next/platform/application_services_roster_availability_batch_read_port.h"
#include "next/platform/application_services_roster_read_port.h"
#include "next/platform/application_services_roster_row_transfer_save_port.h"
#include "next/platform/application_services_selected_class_subtitle_batch_read_port.h"
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
#include <vector>

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

ClassMngr::Next::Application::RosterSnapshot applicationSnapshot(const Roster& roster)
{
    ClassMngr::Next::Application::RosterSnapshot snapshot;
    for (const auto& column : roster.columns)
        snapshot.columns.push_back(Ui::QtTextAdapter::toUtf16String(column));
    for (int width : roster.columnWidths)
        snapshot.columnWidths.push_back(width);
    for (const auto& sourceRow : roster.rows)
    {
        std::vector<std::u16string> row;
        for (const auto& cell : sourceRow)
            row.push_back(Ui::QtTextAdapter::toUtf16String(cell));
        snapshot.rows.push_back(std::move(row));
    }
    return snapshot;
}

QString transferFailureMessage(
    const ClassMngr::Next::Application::RosterRowTransferResult& result)
{
    using namespace ClassMngr::Next::Application;
    if (const auto* error = std::get_if<RosterRowRemovalError>(&result))
        return error->code == RosterRowRemovalErrorCode::InvalidRowIndex
            ? RosterModel::tr("Select a student row to remove.")
            : RosterModel::tr("Selected row is already empty.");
    if (const auto* error = std::get_if<RosterRowTransferPreparationError>(&result))
    {
        switch (error->rejection)
        {
        case RosterRowTransferPreparationRejection::SourceRowHasNoData:
            return RosterModel::tr("Selected row is empty.");
        case RosterRowTransferPreparationRejection::TargetRosterIsFull:
            return RosterModel::tr("Target roster is full.");
        case RosterRowTransferPreparationRejection::DuplicateStudentNamePair:
            return RosterModel::tr("Target roster already contains this student.");
        }
    }
    if (const auto* error =
            std::get_if<ClassMngr::Next::Domain::OperationError>(&result))
        return QString::fromUtf8(error->message.data(),
            static_cast<qsizetype>(error->message.size()));
    return {};
}

std::optional<TransferClassMetadata> transferClassMetadataFromSnapshot(
    const ClassMngr::Next::Application::SelectedClassSubtitleReadSnapshot& subtitle
    )
{
    if (!subtitle.classFields)
    {
        return std::nullopt;
    }

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

    return TransferClassMetadata{
        classInfo.classGrade,
        SidebarNodeNaming::formatClassDisplayName(classInfo, teacher)
    };
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
    if (!loadedSubtitle)
    {
        return {};
    }

    return transferClassMetadataFromSnapshot(loadedSubtitle.value())
        .value_or(TransferClassMetadata{});
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
        QList<Classroom> candidateClasses;
        std::vector<ClassMngr::Next::Domain::ClassId> candidateClassIds;
        candidateClasses.reserve(availableClasses.size());
        candidateClassIds.reserve(
            static_cast<std::size_t>(availableClasses.size())
            );
        for (const Classroom& classroom : availableClasses)
        {
            if (!ClassMngr::Next::Application::shouldReadRosterTransferTargetClassInfo(
                    m_classroom.id,
                    classroom.id
                    ))
            {
                continue;
            }

            const auto typedClassId =
                ClassMngr::Next::Domain::ClassId::fromString(
                    std::to_string(classroom.id)
                    );
            if (!typedClassId)
            {
                continue;
            }

            candidateClasses.append(classroom);
            candidateClassIds.push_back(*typedClassId);
        }

        if (!candidateClassIds.empty())
        {
            ClassMngr::Next::Platform::
                ApplicationServicesSelectedClassSubtitleBatchReadPort
                    subtitleReadPort(m_services);
            const ClassMngr::Next::Application::
                SelectedClassSubtitleBatchReadQuery subtitleQuery(
                    subtitleReadPort
                    );
            const auto subtitles = subtitleQuery.execute(candidateClassIds);
            if (subtitles)
            {
                for (std::size_t index = 0;
                     index < candidateClasses.size();
                     ++index)
                {
                    const Classroom& classroom = candidateClasses.at(
                        static_cast<qsizetype>(index)
                        );
                    const auto targetMetadata =
                        transferClassMetadataFromSnapshot(
                            subtitles.value().at(index)
                            );
                    if (!targetMetadata
                        || !ClassMngr::Next::Application::
                            isRosterTransferTargetEligible(
                                m_classroom.id,
                                Ui::QtTextAdapter::toUtf16String(currentGrade),
                                classroom.id,
                                Ui::QtTextAdapter::toUtf16String(
                                    targetMetadata->grade
                                    )
                                ))
                    {
                        continue;
                    }

                    TransferClassTarget target;
                    target.classId = classroom.id;
                    target.label = targetMetadata->displayName;
                    if (target.label.trimmed().isEmpty())
                    {
                        target.label = classroom.name.trimmed().isEmpty()
                            ? tr("Class %1").arg(classroom.id)
                            : classroom.name.trimmed();
                    }
                    targets.append(std::move(target));
                }
            }
        }

        if (!targets.isEmpty())
        {
            std::vector<ClassMngr::Next::Domain::ClassId> targetClassIds;
            targetClassIds.reserve(static_cast<std::size_t>(targets.size()));
            for (const TransferClassTarget& target : std::as_const(targets))
            {
                const auto typedClassId =
                    ClassMngr::Next::Domain::ClassId::fromString(
                        std::to_string(target.classId)
                        );
                if (typedClassId)
                {
                    targetClassIds.push_back(*typedClassId);
                }
            }

            std::vector<std::u16string> baseColumnNames;
            baseColumnNames.reserve(
                static_cast<std::size_t>(Roster::BaseColumns.size())
                );
            for (const QString& column : Roster::BaseColumns)
            {
                baseColumnNames.push_back(
                    Ui::QtTextAdapter::toUtf16String(column)
                    );
            }

            ClassMngr::Next::Platform::
                ApplicationServicesRosterAvailabilityBatchReadPort
                    availabilityReadPort(m_services);
            const ClassMngr::Next::Application::
                RosterAvailabilityBatchReadQuery availabilityQuery(
                    availabilityReadPort
                    );
            const auto availability = availabilityQuery.execute(
                targetClassIds,
                baseColumnNames
                );
            if (availability
                && availability.value().size()
                    == static_cast<std::size_t>(targets.size()))
            {
                for (std::size_t index = 0; index < targets.size(); ++index)
                {
                    targets[static_cast<qsizetype>(index)].full =
                        availability.value()[index].firstEmptyRow < 0;
                }
            }
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

    const auto sourceId = ClassMngr::Next::Domain::ClassId::fromString(
        std::to_string(m_classroom.id));
    const auto targetId = ClassMngr::Next::Domain::ClassId::fromString(
        std::to_string(targetClassId));
    if (!sourceId || !targetId)
        return;
    std::vector<std::u16string> baseColumns;
    for (const auto& column : Roster::BaseColumns)
        baseColumns.push_back(Ui::QtTextAdapter::toUtf16String(column));

    const ClassMngr::Next::Application::RosterRowTransferRequest request{
        .sourceClassId = *sourceId,
        .targetClassId = *targetId,
        .sourceRoster = applicationSnapshot(currentRosterForSave()),
        .sourceRow = row,
        .baseColumnNames = std::move(baseColumns)
    };
    ClassMngr::Next::Platform::ApplicationServicesRosterReadPort readPort(m_services);
    ClassMngr::Next::Platform::ApplicationServicesRosterRowTransferSavePort savePort(
        m_services);
    const auto transferred =
        ClassMngr::Next::Application::RosterRowTransferUseCase::execute(
            request, readPort, savePort,
            [](std::u16string_view left, std::u16string_view right)
            {
                return Ui::QtTextAdapter::fromUtf16String(left).compare(
                    Ui::QtTextAdapter::fromUtf16String(right),
                    Qt::CaseInsensitive) == 0;
            });
    if (!std::holds_alternative<
            ClassMngr::Next::Application::RosterRowTransferSuccess>(transferred))
    {
        reason = transferFailureMessage(transferred);
        DialogServices::showWarning(this, tr("Cannot Transfer Student"),
            reason.isEmpty() ? tr("The student could not be transferred.") : reason);
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
