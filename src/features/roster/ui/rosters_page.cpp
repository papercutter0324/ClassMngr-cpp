#include "rosters_page.h"

#include "ui/shared/widgets/text_fit_push_button.h"

#include "core/application_services.h"
#include "core/utils/student_name_utils.h"
#include "core/fontmanager.h"
#include "core/utils/sidebar_node_naming.h"
#include "data/data_service.h"
#include "features/roster/ui/roster_column_layout_controller.h"
#include "features/roster/ui/roster_constants.h"
#include "features/roster/ui/roster_header_view.h"
#include "features/roster/ui/roster_item_delegate.h"
#include "features/roster/ui/roster_model.h"
#include "features/roster/ui/roster_print_dialog.h"
#include "features/roster/ui/roster_table_view.h"
#include "features/roster/ui/roster_template_print_service.h"
#include "features/sub_prep/ui/sub_prep_class_navigation.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/widgets/uniform_width_tab_bar.h"

#include <QAction>
#include <QAbstractButton>
#include <QHeaderView>
#include <QHash>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPoint>
#include <QPushButton>
#include <QSizePolicy>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace
{

inline constexpr int AutosaveDelayMs = 750;

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

    const ClassInfo classInfo =
        dataService->loadClassInfo(
            classId
            );

    Teacher teacher;

    if (classInfo.teacherId > 0)
    {
        teacher =
            dataService->getTeacher(
                classInfo.teacherId
                );
    }

    return SidebarNodeNaming::formatClassDisplayName(
        classInfo,
        teacher
        );
}

} // namespace

RostersPage::RostersPage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
{
    setProperty("role", UiRoles::RostersPage);

    buildUi();

    m_autosaveTimer =
        new QTimer(this);
    m_autosaveTimer->setSingleShot(true);
    m_autosaveTimer->setInterval(
        AutosaveDelayMs
        );

    connect(
        m_autosaveTimer,
        &QTimer::timeout,
        this,
        &RostersPage::autosave
        );
}

void RostersPage::loadClass(
    const Classroom& classroom
    )
{
    loadRosters(
        classroom.id
        );
}

void RostersPage::loadRosters(
    int selectedClassId
    )
{
    auto* dataService =
        m_services
            ? m_services->dataService()
            : nullptr;

    if (!dataService || !dataService->isOpen())
    {
        m_rosterClasses.clear();
        rebuildRosterTabs(-1);
        loadRosterClass({});
        setRosterEditorAvailable(false);
        return;
    }

    m_rosterClasses =
        dataService->getClasses();

    int classId =
        selectedClassId > 0
            ? selectedClassId
            : m_classroom.id;

    if (classroomById(classId).id <= 0)
    {
        classId =
            firstRosterClassId();
    }

    rebuildRosterTabs(
        classId
        );

    const Classroom classroom =
        classroomById(classId);

    loadRosterClass(
        classroom
        );

    setRosterEditorAvailable(
        classroom.id > 0
        );
}

void RostersPage::loadRosterClass(
    const Classroom& classroom
    )
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    m_classroom = classroom;
    updateHeaderText();

    m_loadingRoster = true;

    Roster roster;

    if (m_services && m_services->dataService() && m_classroom.id > 0)
    {
        roster =
            m_services
                ->dataService()
                ->loadRoster(m_classroom.id);
    }

    m_model->setRoster(roster);

    QVector<int> normalizedWidths;

    for (int column = 0; column < m_model->columnCount(); ++column)
    {
        const QString columnName =
            m_model->columnName(column);

        int sourceColumn = -1;

        for (int index = 0; index < roster.columns.size(); ++index)
        {
            const bool exactMatch =
                roster.columns[index].compare(columnName, Qt::CaseInsensitive) == 0;

            const bool legacyFallMatch =
                columnName.compare(QStringLiteral("Fall"), Qt::CaseInsensitive) == 0
                && roster.columns[index].compare(QStringLiteral("Autumn"), Qt::CaseInsensitive) == 0;

            if (exactMatch || legacyFallMatch)
            {
                sourceColumn = index;
                break;
            }
        }

        normalizedWidths.append(
            sourceColumn >= 0 && sourceColumn < roster.columnWidths.size()
                ? roster.columnWidths[sourceColumn]
                : 0
            );
    }

    m_layoutController->attach(
        m_table,
        m_model
        );

    m_layoutController->applyWidths(
        normalizedWidths
        );

    for (int row = 0; row < m_model->rowCount(); ++row)
    {
        m_table->setRowHeight(
            row,
            RosterUi::RowHeight
            );
    }

    m_model->clearDirty();
    m_widthsDirty = false;
    m_loadingRoster = false;
    setRosterEditorAvailable(false);
}

void RostersPage::saveData()
{
    saveRosterInternal(true);
}

bool RostersPage::saveChanges()
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    if (!hasUnsavedChanges())
    {
        return true;
    }

    return saveRosterInternal(true);
}

bool RostersPage::hasUnsavedChanges() const
{
    return m_widthsDirty
        || (m_model && m_model->isDirty());
}

void RostersPage::discardChanges()
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    loadClass(m_classroom);
}

QString RostersPage::unsavedChangesTitle() const
{
    return tr("Unsaved Roster Changes");
}

QString RostersPage::unsavedChangesMessage() const
{
    return tr("This roster has unsaved changes.");
}

void RostersPage::retranslateUi()
{
    updateHeaderText();

    if (m_importButton)
    {
        m_importButton->setText(
            tr("Import Scores")
            );
        m_importButton->setToolTip(
            tr("Import final grades from speaking evaluations.")
            );
    }

    if (m_printButton)
    {
        m_printButton->setText(
            tr("Print Rosters")
            );
        m_printButton->setToolTip(
            tr("Print rosters as an A4 PDF.")
            );
    }

    if (m_addColumnButton)
    {
        m_addColumnButton->setText(
            tr("Add Column")
            );
    }

    if (m_removeStudentButton)
    {
        m_removeStudentButton->setText(
            tr("Remove Student")
            );
    }

    if (m_removeColumnButton)
    {
        m_removeColumnButton->setText(
            tr("Remove Column")
            );
    }

    if (m_saveButton)
    {
        m_saveButton->setText(
            tr("Save Changes")
            );
    }

    updateActions();
}

void RostersPage::setSaveMode(
    SaveMode mode
    )
{
    if (m_saveMode == mode)
    {
        return;
    }

    m_saveMode = mode;

    updateActions();

    if (!m_autosaveTimer)
    {
        return;
    }

    if (m_saveMode == SaveMode::Automatic && hasUnsavedChanges())
    {
        m_autosaveTimer->start();
    }
    else
    {
        m_autosaveTimer->stop();
    }
}

bool RostersPage::saveRosterInternal(
    bool showValidationMessages
    )
{
    if (
        !m_services
        || !m_services->dataService()
        || m_classroom.id <= 0
        )
    {
        return false;
    }

    if (!validateRosterBeforeSave(showValidationMessages))
    {
        return false;
    }

    Roster roster =
        currentRosterForSave();

    m_services
        ->dataService()
        ->saveRoster(
            m_classroom.id,
            roster
            );

    m_model->clearDirty();
    m_widthsDirty = false;
    updateActions();

    return true;
}

bool RostersPage::validateRosterBeforeSave(
    bool showValidationMessages
    )
{
    if (!m_model || !m_model->hasDuplicateNameErrors())
    {
        return true;
    }

    if (showValidationMessages)
    {
        QMessageBox message(this);
        message.setIcon(QMessageBox::Warning);
        message.setWindowTitle(
            tr("Duplicate Student Names")
            );
        message.setText(
            tr("Resolve duplicate English/Korean student name pairs before saving.")
            );
        message.setDetailedText(
            m_model->duplicateNameErrorList().join(QLatin1Char('\n'))
            );
        message.exec();
    }

    return false;
}

void RostersPage::addColumn()
{
    bool accepted = false;

    const QString name =
        QInputDialog::getText(
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
        QMessageBox::warning(
            this,
            tr("Cannot Add Column"),
            reason
            );

        return;
    }

    if (!m_model->insertCustomColumn(name))
    {
        return;
    }

    const int column =
        m_model->columnCount() - 1;

    m_layoutController->applyResizeModes();

    m_layoutController->initializeAddedCustomColumn(
        column
        );

    const QModelIndex firstCell =
        m_model->index(
            0,
            column
            );

    m_table->setCurrentIndex(firstCell);
    m_table->edit(firstCell);

    scheduleAutosave();
    updateActions();
}

void RostersPage::removeColumn()
{
    const QModelIndex current =
        m_table->currentIndex();

    QString reason;

    if (!m_model->canRemoveColumn(current.column(), &reason))
    {
        QMessageBox::warning(
            this,
            tr("Cannot Remove Column"),
            reason
            );

        return;
    }

    const QString columnName =
        m_model->columnName(
            current.column()
            );

    const QMessageBox::StandardButton response =
        QMessageBox::question(
            this,
            tr("Remove Column"),
            tr("Remove the \"%1\" column?").arg(columnName)
            );

    if (response != QMessageBox::Yes)
    {
        return;
    }

    const int removedWidth =
        m_table->columnWidth(
            current.column()
            );

    m_model->removeRosterColumn(
        current.column()
        );

    m_layoutController->applyResizeModes();
    m_layoutController->handleCustomColumnRemoved(
        removedWidth
        );
    scheduleAutosave();
    updateActions();
}

void RostersPage::removeStudent()
{
    if (!m_table)
    {
        return;
    }

    const QModelIndex current =
        m_table->currentIndex();

    removeRosterRow(
        current.row()
        );
}

void RostersPage::moveStudentRow(
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

    const bool moved =
        m_model->moveRosterRow(
            sourceRow,
            destinationRow
            );

    m_movingRosterRow = false;

    if (!moved)
    {
        return;
    }

    selectRosterCell(
        destinationRow,
        qBound(
            0,
            currentColumn,
            m_model->columnCount() - 1
            )
        );

    scheduleAutosave();
    updateActions();
}

void RostersPage::importScores()
{
    if (
        !m_services
        || !m_services->dataService()
        || m_classroom.id <= 0
        )
    {
        return;
    }

    const auto findModelColumn =
        [this](const QString& name)
        {
            for (int column = 0; column < m_model->columnCount(); ++column)
            {
                if (m_model->columnName(column).compare(name, Qt::CaseInsensitive) == 0)
                {
                    return column;
                }
            }

            return -1;
        };

    const int englishColumn =
        findModelColumn(QStringLiteral("English"));

    const int koreanColumn =
        findModelColumn(QStringLiteral("Korean"));

    if (englishColumn < 0 || koreanColumn < 0)
    {
        QMessageBox::warning(
            this,
            tr("Import Scores"),
            tr("Roster must contain 'English' and 'Korean' columns.")
            );
        return;
    }

    const QStringList evaluationColumns{
        QStringLiteral("Winter"),
        QStringLiteral("Speech Contest"),
        QStringLiteral("Summer"),
        QStringLiteral("Fall")
    };

    int changeCount = 0;

    for (const QString& evaluationName : evaluationColumns)
    {
        const int scoreColumn =
            findModelColumn(evaluationName);

        if (scoreColumn < 0)
        {
            continue;
        }

        const QList<SpeakingEvalScore> scores =
            m_services
                ->dataService()
                ->buildRosterScoreImport(
                    m_classroom.id,
                    evaluationName
                    );

        if (scores.isEmpty())
        {
            continue;
        }

        QHash<QString, QString> lookup;

        for (const SpeakingEvalScore& score : scores)
        {
            lookup.insert(
                StudentNameUtils::namePairKey(
                    score.englishName,
                    score.koreanName
                    ),
                score.finalGrade
                );
        }

        for (int row = 0; row < m_model->rowCount(); ++row)
        {
            const QString englishName =
                m_model
                    ->index(
                        row,
                        englishColumn
                        )
                    .data(Qt::EditRole)
                    .toString()
                    .trimmed();

            const QString koreanName =
                m_model
                    ->index(
                        row,
                        koreanColumn
                        )
                    .data(Qt::EditRole)
                    .toString()
                    .trimmed();

            const QString namePairKey =
                StudentNameUtils::namePairKey(
                    englishName,
                    koreanName
                    );

            if (namePairKey.isEmpty())
            {
                continue;
            }

            if (!lookup.contains(namePairKey))
            {
                continue;
            }

            const QString finalGrade = lookup.value(namePairKey);

            const QModelIndex index =
                m_model->index(
                    row,
                    scoreColumn
                    );

            if (
                !index.isValid()
                || index.data(Qt::EditRole).toString() == finalGrade
                )
            {
                continue;
            }

            if (
                m_model->setData(
                    index,
                    finalGrade,
                    Qt::EditRole
                    )
                )
            {
                ++changeCount;
            }
        }
    }

    if (changeCount == 0)
    {
        QMessageBox::information(
            this,
            tr("Import Scores"),
            tr("Scores are already up to date.")
            );
        return;
    }

    updateActions();
    scheduleAutosave();

    QMessageBox::information(
        this,
        tr("Import Scores"),
        tr("Scores imported successfully.")
    );
}

void RostersPage::printRosters()
{
    if (hasUnsavedChanges() && !saveChanges())
    {
        return;
    }

    RosterPrintDialog dialog(
        m_services,
        m_classroom.id,
        RosterTemplatePrintService::Scope::AllClasses,
        this
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    RosterTemplatePrintService::Request request;
    request.parent = this;
    request.services = m_services;
    request.currentClassId = m_classroom.id;
    request.scope = dialog.selectedScope();
    request.selectedClassIds = dialog.selectedClassIds();
    request.templateId = dialog.selectedTemplateId();
    request.selectedExtraColumns = dialog.selectedExtraColumns();
    request.perClassExtraInfoOrientation =
        dialog.selectedPerClassExtraInfoOrientation();

    RosterTemplatePrintService::Result result;

    switch (dialog.selectedAction())
    {
    case RosterPrintDialog::Action::SaveAs:
        result =
            RosterTemplatePrintService::saveRostersPdf(
                request,
                dialog.selectedSavePath()
                );
        break;

    case RosterPrintDialog::Action::Print:
    default:
        result =
            RosterTemplatePrintService::printRosters(
                request
                );
        break;
    }

    if (result.status == RosterTemplatePrintService::Status::Failed)
    {
        QMessageBox::warning(
            this,
            tr("Print Rosters"),
            result.message
            );
    }
}

void RostersPage::autosave()
{
    if (!hasUnsavedChanges())
    {
        return;
    }

    saveRosterInternal(false);
}

void RostersPage::updateActions()
{
    if (
        !m_saveButton
        || !m_importButton
        || !m_addColumnButton
        || !m_removeStudentButton
        || !m_removeColumnButton
        )
    {
        return;
    }

    const bool hasActiveClass =
        m_classroom.id > 0;

    const bool showSaveButton =
        m_saveMode != SaveMode::Automatic;

    m_saveButton->setVisible(
        showSaveButton
        );

    m_saveButton->setEnabled(
        showSaveButton
        && hasUnsavedChanges()
        && hasActiveClass
        );

    m_importButton->setEnabled(
        hasActiveClass
        );

    if (m_printButton)
    {
        m_printButton->setEnabled(
            !m_rosterClasses.isEmpty()
            );
    }

    m_addColumnButton->setEnabled(
        hasActiveClass
        );

    QString reason;

    m_removeColumnButton->setEnabled(
        hasActiveClass
        && m_model
        && m_model->canRemoveColumn(
            m_table->currentIndex().column(),
            &reason
            )
        );

    m_removeStudentButton->setEnabled(
        hasActiveClass
        && m_model
        && m_model->canRemoveRow(
            m_table->currentIndex().row(),
            &reason
            )
        );
}

void RostersPage::showRosterContextMenu(
    const QPoint& position
    )
{
    if (!m_table || !m_model)
    {
        return;
    }

    const QModelIndex clicked =
        m_table->indexAt(position);

    if (!clicked.isValid())
    {
        return;
    }

    m_table->setCurrentIndex(clicked);

    QString reason;

    const bool canRemove =
        m_model->canRemoveRow(
            clicked.row(),
            &reason
            );

    QMenu menu(this);

    QAction* removeAction =
        menu.addAction(
            tr("Remove Student")
            );

    removeAction->setEnabled(canRemove);

    if (!canRemove && !reason.isEmpty())
    {
        removeAction->setToolTip(reason);
    }

    QMenu* transferMenu =
        menu.addMenu(
            tr("Transfer Class")
            );

    transferMenu->setEnabled(canRemove);

    QHash<QAction*, int> transferActions;

    auto* dataService =
        m_services
            ? m_services->dataService()
            : nullptr;

    const QString currentGrade =
        dataService && dataService->isOpen() && m_classroom.id > 0
            ? dataService
                ->loadClassInfo(
                    m_classroom.id
                    )
                .classGrade
                .trimmed()
            : QString();

    QList<TransferClassTarget> targets;

    if (
        canRemove
        && dataService
        && dataService->isOpen()
        && !currentGrade.isEmpty()
        )
    {
        for (const Classroom& classroom : dataService->getClasses())
        {
            if (
                classroom.id <= 0
                || classroom.id == m_classroom.id
                )
            {
                continue;
            }

            const ClassInfo targetInfo =
                dataService->loadClassInfo(
                    classroom.id
                    );

            if (targetInfo.classGrade.trimmed() != currentGrade)
            {
                continue;
            }

            RosterModel targetModel;
            targetModel.setRoster(
                dataService->loadRoster(
                    classroom.id
                    )
                );

            TransferClassTarget target;
            target.classId =
                classroom.id;
            target.label =
                sidebarClassDisplayName(
                    dataService,
                    classroom.id
                    );

            if (target.label.trimmed().isEmpty())
            {
                target.label =
                    classroom.name.trimmed().isEmpty()
                        ? tr("Class %1").arg(classroom.id)
                        : classroom.name.trimmed();
            }

            target.full =
                targetModel.firstEmptyRow() < 0;

            targets.append(target);
        }

        std::sort(
            targets.begin(),
            targets.end(),
            [](const TransferClassTarget& left, const TransferClassTarget& right)
            {
                const int comparison =
                    QString::localeAwareCompare(
                        left.label,
                        right.label
                        );

                if (comparison != 0)
                {
                    return comparison < 0;
                }

                return left.classId < right.classId;
            }
            );
    }

    if (targets.isEmpty())
    {
        QAction* emptyAction =
            transferMenu->addAction(
                tr("No same-grade classes")
                );
        emptyAction->setEnabled(false);
    }
    else
    {
        for (const TransferClassTarget& target : std::as_const(targets))
        {
            QAction* transferAction =
                transferMenu->addAction(
                    target.full
                        ? tr("%1 (full)").arg(target.label)
                        : target.label
                    );

            transferAction->setEnabled(
                !target.full
                );

            if (target.full)
            {
                transferAction->setToolTip(
                    tr("Target roster is full.")
                    );
            }
            else
            {
                transferActions.insert(
                    transferAction,
                    target.classId
                    );
            }
        }
    }

    QAction* selectedAction =
        menu.exec(
            m_table->viewport()->mapToGlobal(position)
            );

    if (selectedAction == removeAction && canRemove)
    {
        removeRosterRow(
            clicked.row()
            );
    }
    else if (transferActions.contains(selectedAction))
    {
        transferRosterRow(
            clicked.row(),
            transferActions.value(selectedAction)
            );
    }
}

void RostersPage::buildUi()
{
    contentLayout()->setContentsMargins(
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        0
        );

    contentLayout()->setSpacing(
        UiConstants::Pages::Spacing
        );

    auto* headerLayout =
        new QVBoxLayout;

    headerLayout->setContentsMargins(
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin
        );

    headerLayout->setSpacing(
        UiConstants::Pages::HeaderSpacing
        );

    m_titleLabel =
        new QLabel(
            tr("Rosters"),
            this
            );

    m_titleLabel->setObjectName("pageTitle");
    m_titleLabel->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::TitleFontSize,
            QFont::Bold
            )
        );

    m_subtitleLabel =
        new QLabel(
            tr("No class selected"),
            this
            );

    m_subtitleLabel->setObjectName("pageSubtitle");
    m_subtitleLabel->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::SubtitleFontSize
            )
        );

    headerLayout->addWidget(m_titleLabel);
    headerLayout->addWidget(m_subtitleLabel);

    contentLayout()->addLayout(headerLayout);
    contentLayout()->addSpacing(
        UiConstants::Pages::HeaderContentSpacing
        );

    m_tabsContainer =
        new QWidget(this);
    m_tabsContainer->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Maximum
        );
    m_tabsLayout =
        new QVBoxLayout(m_tabsContainer);
    m_tabsLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );
    m_tabsLayout->setSpacing(8);

    contentLayout()->addWidget(
        m_tabsContainer
        );

    m_model =
        new RosterModel(this);

    m_layoutController =
        new RosterColumnLayoutController(this);

    m_table =
        new RosterTableView(this);

    m_header =
        new RosterHeaderView(
            Qt::Horizontal,
            m_table
            );

    m_header->setLayoutController(
        m_layoutController
        );

    m_table->setHorizontalHeader(m_header);
    m_table->setModel(m_model);
    m_table->setLayoutController(m_layoutController);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);

    m_layoutController->attach(
        m_table,
        m_model
        );

    m_delegate =
        new RosterItemDelegate(
            m_layoutController,
            this
            );

    m_table->setItemDelegate(m_delegate);
    m_table->verticalHeader()->setDefaultSectionSize(RosterUi::RowHeight);

    m_layoutController->applyWidths({});

    m_emptyLabel =
        new QLabel(
            tr("No classes available"),
            this
            );
    m_emptyLabel->setObjectName("pageSubtitle");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setFont(
        FontManager::getUiFont(12)
        );

    contentLayout()->addWidget(
        m_emptyLabel
        );
    contentLayout()->addWidget(
        m_table,
        1
        );

    m_importButton =
        new TextFitPushButton(
            tr("Import Scores"),
            this
            );

    m_importButton->setEnabled(true);
    m_importButton->setToolTip(
        tr("Import final grades from speaking evaluations.")
        );

    m_printButton =
        new TextFitPushButton(
            tr("Print Rosters"),
            this
            );

    m_printButton->setToolTip(
        tr("Print rosters as an A4 PDF.")
        );

    m_addColumnButton =
        new TextFitPushButton(
            tr("Add Column"),
            this
            );

    m_removeStudentButton =
        new TextFitPushButton(
            tr("Remove Student"),
            this
            );

    m_removeColumnButton =
        new TextFitPushButton(
            tr("Remove Column"),
            this
            );

    m_saveButton =
        new TextFitPushButton(
            tr("Save Changes"),
            this
            );

    bottomLayout()->addWidget(m_importButton);
    bottomLayout()->addWidget(m_printButton);
    bottomLayout()->addStretch();
    bottomLayout()->addWidget(m_removeStudentButton);
    bottomLayout()->addWidget(m_addColumnButton);
    bottomLayout()->addWidget(m_removeColumnButton);
    bottomLayout()->addWidget(m_saveButton);

    connect(
        m_addColumnButton,
        &QPushButton::clicked,
        this,
        &RostersPage::addColumn
        );

    connect(
        m_removeStudentButton,
        &QPushButton::clicked,
        this,
        &RostersPage::removeStudent
        );

    connect(
        m_removeColumnButton,
        &QPushButton::clicked,
        this,
        &RostersPage::removeColumn
        );

    connect(
        m_saveButton,
        &QPushButton::clicked,
        this,
        &RostersPage::saveData
        );

    connect(
        m_importButton,
        &QPushButton::clicked,
        this,
        &RostersPage::importScores
        );

    connect(
        m_printButton,
        &QPushButton::clicked,
        this,
        &RostersPage::printRosters
        );

    connect(
        m_table,
        &QWidget::customContextMenuRequested,
        this,
        &RostersPage::showRosterContextMenu
        );

    connect(
        m_table,
        &RosterTableView::rowMoveRequested,
        this,
        &RostersPage::moveStudentRow
        );

    connect(
        m_model,
        &RosterModel::dirtyChanged,
        this,
        [this](bool)
        {
            updateActions();
            scheduleAutosave();
        }
        );

    connect(
        m_model,
        &QAbstractItemModel::dataChanged,
        this,
        [this](
            const QModelIndex& topLeft,
            const QModelIndex& bottomRight,
            const QList<int>&
            )
        {
            handleNameCellChanged(
                topLeft,
                bottomRight
                );

            scheduleAutosave();
        }
        );

    connect(
        m_table->selectionModel(),
        &QItemSelectionModel::currentChanged,
        this,
        &RostersPage::updateActions
        );

    connect(
        m_table->horizontalHeader(),
        &QHeaderView::sectionResized,
        this,
        [this](int logicalIndex, int, int)
        {
            if (m_loadingRoster)
            {
                return;
            }

            m_layoutController->handleSectionResized(
                logicalIndex
                );

            m_widthsDirty = true;
            scheduleAutosave();
            updateActions();
        }
        );

    updateActions();
}

void RostersPage::rebuildRosterTabs(
    int selectedClassId
    )
{
    if (!m_tabsLayout || !m_tabsContainer)
    {
        return;
    }

    m_rebuildingRosterTabs = true;

    while (QLayoutItem* item = m_tabsLayout->takeAt(0))
    {
        if (QWidget* widget = item->widget())
        {
            widget->deleteLater();
        }

        delete item;
    }

    m_rosterTabs = nullptr;

    auto* dataService =
        m_services
            ? m_services->dataService()
            : nullptr;

    QList<SubPrepClassNavigation::ClassEntry> entries;

    if (dataService)
    {
        for (const Classroom& classroom : std::as_const(m_rosterClasses))
        {
            if (classroom.id <= 0)
            {
                continue;
            }

            const ClassInfo info =
                dataService->loadClassInfo(
                    classroom.id
                    );

            Teacher teacher;

            if (info.teacherId > 0)
            {
                teacher =
                    dataService->getTeacher(
                        info.teacherId
                        );
            }

            SubPrepClassNavigation::ClassEntry entry;
            entry.classId =
                classroom.id;
            entry.classroomName =
                classroom.name;
            entry.grade =
                info.classGrade;
            entry.level =
                info.classLevel;
            entry.regularTimes =
                info.classTimes;
            entry.intensiveTimes =
                info.intensiveTimes;
            entry.teacherEn =
                teacher.teacherEn;
            entry.teacherKr =
                teacher.teacherKr;

            entries.append(entry);
        }
    }

    const SubPrepClassNavigation::Model navigation =
        SubPrepClassNavigation::build(
            entries
            );

    const auto createTabPage =
        [](QWidget* parent, int classId)
        {
            auto* page =
                new QWidget(parent);
            page->setProperty(
                "class_id",
                classId
                );
            return page;
        };

    const auto connectClassTabs =
        [this](QTabWidget* tabs)
        {
            connect(
                tabs,
                &QTabWidget::currentChanged,
                this,
                [this, tabs](int)
                {
                    if (m_rebuildingRosterTabs || m_restoringRosterTabs)
                    {
                        return;
                    }

                    activateRosterClass(
                        currentClassIdFromTabs(tabs)
                        );
                }
                );
        };

    if (navigation.mode == SubPrepClassNavigation::Mode::Flat)
    {
        auto* tabs =
            new UniformWidthTabWidget(
                UniformWidthTabKind::Class,
                QStringLiteral("rosterClassTabBar"),
                m_tabsContainer
                );
        tabs->setSizePolicy(
            QSizePolicy::Expanding,
            QSizePolicy::Maximum
            );
        tabs->setObjectName("rosterClassTabs");

        for (const SubPrepClassNavigation::ClassTab& tab
             : navigation.flatClasses)
        {
            tabs->addTab(
                createTabPage(
                    tabs,
                    tab.classId
                    ),
                tab.label
                );
        }

        connectClassTabs(tabs);

        m_rosterTabs =
            tabs;
        m_tabsLayout->addWidget(
            tabs
            );
    }
    else
    {
        auto* gradeTabs =
            new UniformWidthTabWidget(
                UniformWidthTabKind::Grade,
                QStringLiteral("rosterGradeTabBar"),
                m_tabsContainer
                );
        gradeTabs->setSizePolicy(
            QSizePolicy::Expanding,
            QSizePolicy::Maximum
            );
        gradeTabs->setObjectName("rosterGradeTabs");

        for (const SubPrepClassNavigation::GradeGroup& group
             : navigation.gradeGroups)
        {
            auto* gradePage =
                new QWidget(gradeTabs);

            auto* gradeLayout =
                new QVBoxLayout(gradePage);
            gradeLayout->setContentsMargins(0, 0, 0, 0);
            gradeLayout->setSpacing(8);
            gradeLayout->setAlignment(Qt::AlignTop);

            auto* classTabs =
                new UniformWidthTabWidget(
                    UniformWidthTabKind::Class,
                    QStringLiteral("rosterClassTabBar"),
                    gradePage
                    );
            classTabs->setSizePolicy(
                QSizePolicy::Expanding,
                QSizePolicy::Maximum
                );
            classTabs->setObjectName("rosterClassTabs");

            for (const SubPrepClassNavigation::ClassTab& tab
                 : group.classes)
            {
                classTabs->addTab(
                    createTabPage(
                        classTabs,
                        tab.classId
                        ),
                    tab.label
                    );
            }

            connectClassTabs(classTabs);

            gradeLayout->addWidget(
                classTabs
                );
            gradeTabs->addTab(
                gradePage,
                group.label
                );
        }

        connect(
            gradeTabs,
            &QTabWidget::currentChanged,
            this,
            [this, gradeTabs](int)
            {
                if (m_rebuildingRosterTabs || m_restoringRosterTabs)
                {
                    return;
                }

                activateRosterClass(
                    currentClassIdFromTabs(gradeTabs)
                    );
            }
            );

        m_rosterTabs =
            gradeTabs;
        m_tabsLayout->addWidget(
            gradeTabs
            );
    }

    m_tabsContainer->setVisible(
        m_rosterTabs && m_rosterTabs->count() > 0
        );

    syncTabWidgetToClass(
        m_rosterTabs,
        selectedClassId
        );

    m_rebuildingRosterTabs = false;
}

bool RostersPage::activateRosterClass(
    int classId
    )
{
    if (
        classId <= 0
        || m_rebuildingRosterTabs
        || m_restoringRosterTabs
        )
    {
        return false;
    }

    if (classId == m_classroom.id)
    {
        return true;
    }

    if (m_classroom.id > 0 && !saveChanges())
    {
        restoreRosterTabSelection();
        return false;
    }

    const Classroom classroom =
        classroomById(classId);

    if (classroom.id <= 0)
    {
        restoreRosterTabSelection();
        return false;
    }

    loadRosterClass(
        classroom
        );
    setRosterEditorAvailable(true);

    return true;
}

void RostersPage::restoreRosterTabSelection()
{
    if (!m_rosterTabs)
    {
        return;
    }

    m_restoringRosterTabs = true;
    syncTabWidgetToClass(
        m_rosterTabs,
        m_classroom.id
        );
    m_restoringRosterTabs = false;
}

void RostersPage::syncTabWidgetToClass(
    QTabWidget* tabs,
    int classId
    )
{
    if (!tabs)
    {
        return;
    }

    for (int index = 0; index < tabs->count(); ++index)
    {
        QWidget* page =
            tabs->widget(index);

        if (
            page
            && page->property("class_id").toInt() == classId
            )
        {
            tabs->setCurrentIndex(index);
            return;
        }

        auto* nestedTabs =
            page
                ? page->findChild<QTabWidget*>(
                    QStringLiteral("rosterClassTabs")
                    )
                : nullptr;

        if (!nestedTabs)
        {
            continue;
        }

        for (int childIndex = 0; childIndex < nestedTabs->count(); ++childIndex)
        {
            QWidget* childPage =
                nestedTabs->widget(childIndex);

            if (
                childPage
                && childPage->property("class_id").toInt() == classId
                )
            {
                tabs->setCurrentIndex(index);
                nestedTabs->setCurrentIndex(childIndex);
                return;
            }
        }
    }

    if (tabs->count() > 0)
    {
        tabs->setCurrentIndex(0);

        if (auto* nestedTabs =
                tabs->currentWidget()
                    ? tabs->currentWidget()->findChild<QTabWidget*>(
                        QStringLiteral("rosterClassTabs")
                        )
                    : nullptr)
        {
            nestedTabs->setCurrentIndex(0);
        }
    }
}

int RostersPage::currentClassIdFromTabs(
    QTabWidget* tabs
    ) const
{
    if (!tabs || tabs->currentIndex() < 0)
    {
        return -1;
    }

    QWidget* page =
        tabs->currentWidget();

    const int pageClassId =
        page
            ? page->property("class_id").toInt()
            : -1;

    if (pageClassId > 0)
    {
        return pageClassId;
    }

    auto* nestedTabs =
        page
            ? page->findChild<QTabWidget*>(
                QStringLiteral("rosterClassTabs")
                )
            : nullptr;

    if (!nestedTabs || nestedTabs->currentIndex() < 0)
    {
        return -1;
    }

    QWidget* nestedPage =
        nestedTabs->currentWidget();

    return nestedPage
        ? nestedPage->property("class_id").toInt()
        : -1;
}

Classroom RostersPage::classroomById(
    int classId
    ) const
{
    for (const Classroom& classroom : m_rosterClasses)
    {
        if (classroom.id == classId)
        {
            return classroom;
        }
    }

    return {};
}

int RostersPage::firstRosterClassId() const
{
    for (const Classroom& classroom : m_rosterClasses)
    {
        if (classroom.id > 0)
        {
            return classroom.id;
        }
    }

    return -1;
}

void RostersPage::setRosterEditorAvailable(
    bool available
    )
{
    if (m_emptyLabel)
    {
        m_emptyLabel->setVisible(
            !available
            );
    }

    if (m_table)
    {
        m_table->setVisible(
            available
            );
        m_table->setEnabled(
            available
            );
    }

    updateActions();
}

void RostersPage::updateHeaderText()
{
    m_titleLabel->setText(
        tr("Rosters")
        );

    if (m_classroom.id <= 0)
    {
        m_subtitleLabel->setText(
            tr("No classes available")
            );
        return;
    }

    const QString sidebarName =
        sidebarClassDisplayName(
            m_services
                ? m_services->dataService()
                : nullptr,
            m_classroom.id
            );

    if (!sidebarName.isEmpty())
    {
        m_subtitleLabel->setText(
            sidebarName
            );
        return;
    }

    m_subtitleLabel->setText(
        m_classroom.name.trimmed().isEmpty()
            ? tr("Class %1").arg(m_classroom.id)
            : m_classroom.name.trimmed()
        );
}

void RostersPage::handleNameCellChanged(
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
                resolveDuplicateName(
                    row,
                    column
                    );
                return;
            }
        }
    }
}

void RostersPage::scheduleAutosave()
{
    if (
        m_loadingRoster
        || m_saveMode != SaveMode::Automatic
        || !m_autosaveTimer
        || m_classroom.id <= 0
        )
    {
        return;
    }

    m_autosaveTimer->start();
}

void RostersPage::resolveDuplicateName(
    int row,
    int editedColumn
    )
{
    if (
        !m_model
        || !m_table
        || !m_model->isNameColumn(editedColumn)
        )
    {
        return;
    }

    const QList<int> duplicateRows =
        m_model->duplicateNameRows(row);

    if (duplicateRows.isEmpty())
    {
        return;
    }

    QStringList duplicateRowLabels;

    for (int duplicateRow : duplicateRows)
    {
        duplicateRowLabels.append(
            QString::number(duplicateRow + 1)
            );
    }

    const int koreanColumn =
        m_model->koreanNameColumn();

    const QString suggestedName =
        m_model->suggestedKoreanNameWithSuffix(row);

    QMessageBox dialog(this);
    dialog.setIcon(QMessageBox::Warning);
    dialog.setWindowTitle(
        tr("Duplicate Student Name")
        );
    dialog.setText(
        tr("This English/Korean name combination already exists.")
        );
    dialog.setInformativeText(
        tr("Duplicate row(s): %1").arg(
            duplicateRowLabels.join(QStringLiteral(", "))
            )
        );

    QPushButton* suffixButton =
        dialog.addButton(
            suggestedName.isEmpty()
                ? tr("No Suffix Available")
                : tr("Use %1").arg(suggestedName),
            QMessageBox::AcceptRole
            );

    suffixButton->setEnabled(
        !suggestedName.isEmpty()
        );

    QPushButton* clearButton =
        dialog.addButton(
            tr("Clear Edited Cell"),
            QMessageBox::DestructiveRole
            );

    QPushButton* locateButton =
        dialog.addButton(
            tr("Locate Duplicate"),
            QMessageBox::ActionRole
            );

    QPushButton* keepButton =
        dialog.addButton(
            tr("Keep As-Is"),
            QMessageBox::RejectRole
            );

    if (!suggestedName.isEmpty())
    {
        dialog.setDefaultButton(suffixButton);
    }
    else
    {
        dialog.setDefaultButton(clearButton);
    }

    dialog.exec();

    QAbstractButton* clickedButton =
        dialog.clickedButton();

    if (clickedButton == suffixButton && !suggestedName.isEmpty())
    {
        m_resolvingDuplicateName = true;

        m_model->setData(
            m_model->index(
                row,
                koreanColumn
                ),
            suggestedName,
            Qt::EditRole
            );

        m_resolvingDuplicateName = false;

        selectRosterCell(
            row,
            koreanColumn
            );
    }
    else if (clickedButton == clearButton)
    {
        m_resolvingDuplicateName = true;

        m_model->setData(
            m_model->index(
                row,
                editedColumn
                ),
            QString(),
            Qt::EditRole
            );

        m_resolvingDuplicateName = false;

        selectRosterCell(
            row,
            editedColumn
            );
    }
    else if (clickedButton == locateButton)
    {
        selectRosterCell(
            duplicateRows.first(),
            m_model->englishNameColumn()
            );
    }
    else
    {
        Q_UNUSED(keepButton);
    }

    m_table->viewport()->update();
    updateActions();
}

void RostersPage::selectRosterCell(
    int row,
    int column
    )
{
    if (!m_model || !m_table)
    {
        return;
    }

    const QModelIndex index =
        m_model->index(
            row,
            column
            );

    if (!index.isValid())
    {
        return;
    }

    m_table->setCurrentIndex(index);
    m_table->scrollTo(index);
}

bool RostersPage::removeRosterRow(
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
        QMessageBox::warning(
            this,
            tr("Cannot Remove Student"),
            reason
            );

        return false;
    }

    const QString label =
        rosterRowLabel(row);

    const QString message =
        label.isEmpty()
            ? tr("Remove row %1 from the roster?").arg(row + 1)
            : tr("Remove %1 from the roster?").arg(label);

    const QMessageBox::StandardButton response =
        QMessageBox::question(
            this,
            tr("Remove Student"),
            message
            );

    if (response != QMessageBox::Yes)
    {
        return false;
    }

    m_removingRosterRow = true;

    const bool removed =
        m_model->removeRosterRow(row);

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
        selectRosterCell(
            nextRow,
            0
            );
    }

    scheduleAutosave();
    updateActions();

    return true;
}

void RostersPage::transferRosterRow(
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
        QMessageBox::warning(
            this,
            tr("Cannot Transfer Student"),
            reason
            );

        return;
    }

    if (!validateRosterBeforeSave(true))
    {
        return;
    }

    auto* dataService =
        m_services->dataService();

    const QStringList sourceColumns =
        m_model->columnNames();

    const QStringList sourceRow =
        m_model->rowValues(row);

    const Roster targetSourceRoster =
        dataService->loadRoster(
            targetClassId
            );

    RosterModel targetModel;
    targetModel.setRoster(
        targetSourceRoster
        );

    reason.clear();

    if (
        !targetModel.insertTransferredRow(
            sourceColumns,
            sourceRow,
            &reason
            )
        )
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

    Roster targetRoster =
        targetModel.toRoster();

    targetRoster.columnWidths =
        normalizedColumnWidths(
            targetSourceRoster,
            targetRoster.columns
            );

    const Roster sourceRoster =
        rosterWithRowRemoved(row);

    const bool saved =
        dataService->saveRosters(
            {
                qMakePair(
                    m_classroom.id,
                    sourceRoster
                    ),
                qMakePair(
                    targetClassId,
                    targetRoster
                    )
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

    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    m_removingRosterRow = true;
    m_model->removeRosterRow(row);
    m_removingRosterRow = false;

    m_model->clearDirty();
    m_widthsDirty = false;

    const int nextRow =
        row < m_model->rowCount()
            ? row
            : m_model->rowCount() - 1;

    if (nextRow >= 0 && m_model->columnCount() > 0)
    {
        selectRosterCell(
            nextRow,
            0
            );
    }

    updateActions();
}

Roster RostersPage::currentRosterForSave() const
{
    Roster roster =
        m_model
            ? m_model->toRoster()
            : Roster();

    roster.columnWidths =
        m_layoutController
            ? m_layoutController->currentWidths()
            : QVector<int>();

    return roster;
}

Roster RostersPage::rosterWithRowRemoved(
    int row
    ) const
{
    Roster roster =
        currentRosterForSave();

    if (row < 0 || row >= roster.rows.size())
    {
        return roster;
    }

    const int lastRow =
        roster.rows.size() - 1;

    for (int sourceRow = row + 1; sourceRow <= lastRow; ++sourceRow)
    {
        roster.rows[sourceRow - 1] =
            roster.rows[sourceRow];
    }

    roster.rows[lastRow] =
        QStringList(
            roster.columns.size(),
            QString()
            );

    return roster;
}

QVector<int> RostersPage::normalizedColumnWidths(
    const Roster& roster,
    const QStringList& columns
    ) const
{
    QVector<int> widths;
    widths.reserve(
        columns.size()
        );

    for (const QString& columnName : columns)
    {
        int sourceColumn = -1;

        for (int index = 0; index < roster.columns.size(); ++index)
        {
            const bool exactMatch =
                roster.columns[index].compare(columnName, Qt::CaseInsensitive) == 0;

            const bool legacyFallMatch =
                columnName.compare(QStringLiteral("Fall"), Qt::CaseInsensitive) == 0
                && roster.columns[index].compare(QStringLiteral("Autumn"), Qt::CaseInsensitive) == 0;

            if (exactMatch || legacyFallMatch)
            {
                sourceColumn = index;
                break;
            }
        }

        widths.append(
            sourceColumn >= 0 && sourceColumn < roster.columnWidths.size()
                ? roster.columnWidths[sourceColumn]
                : 0
            );
    }

    return widths;
}

QString RostersPage::rosterRowLabel(
    int row
    ) const
{
    if (!m_model || row < 0 || row >= m_model->rowCount())
    {
        return {};
    }

    QStringList names;

    const int englishColumn =
        m_model->englishNameColumn();

    if (englishColumn >= 0)
    {
        const QString englishName =
            m_model
                ->index(
                    row,
                    englishColumn
                    )
                .data(Qt::DisplayRole)
                .toString()
                .trimmed();

        if (!englishName.isEmpty())
        {
            names.append(englishName);
        }
    }

    const int koreanColumn =
        m_model->koreanNameColumn();

    if (koreanColumn >= 0)
    {
        const QString koreanName =
            m_model
                ->index(
                    row,
                    koreanColumn
                    )
                .data(Qt::DisplayRole)
                .toString()
                .trimmed();

        if (!koreanName.isEmpty())
        {
            names.append(koreanName);
        }
    }

    return names.join(
        QStringLiteral(" / ")
        );
}
