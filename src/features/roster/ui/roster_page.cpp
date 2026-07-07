#include "roster_page.h"

#include "ui/shared/widgets/text_fit_push_button.h"

#include "core/application_services.h"
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
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"

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
#include <QPair>
#include <QPoint>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>

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

RosterPage::RosterPage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
{
    setProperty("role", UiRoles::RosterPage);

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
        &RosterPage::autosave
        );
}

void RosterPage::loadClass(
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
    updateActions();
}

void RosterPage::saveData()
{
    saveRosterInternal(true);
}

bool RosterPage::saveChanges()
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

bool RosterPage::hasUnsavedChanges() const
{
    return m_widthsDirty
        || (m_model && m_model->isDirty());
}

void RosterPage::discardChanges()
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    loadClass(m_classroom);
}

QString RosterPage::unsavedChangesTitle() const
{
    return tr("Unsaved Roster Changes");
}

QString RosterPage::unsavedChangesMessage() const
{
    return tr("This roster has unsaved changes.");
}

void RosterPage::retranslateUi()
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
            tr("Print rosters from a spreadsheet template.")
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

void RosterPage::setSaveMode(
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

bool RosterPage::saveRosterInternal(
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

bool RosterPage::validateRosterBeforeSave(
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

void RosterPage::addColumn()
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

void RosterPage::removeColumn()
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

void RosterPage::removeStudent()
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

void RosterPage::moveStudentRow(
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

void RosterPage::importScores()
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

    const auto keyFor =
        [](const QString& englishName, const QString& koreanName)
        {
            return englishName.trimmed()
                + QChar(0x001F)
                + koreanName.trimmed();
        };

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
                keyFor(
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

            if (englishName.isEmpty() || koreanName.isEmpty())
            {
                continue;
            }

            const QString finalGrade =
                lookup.value(
                    keyFor(
                        englishName,
                        koreanName
                        )
                    );

            if (finalGrade.isEmpty())
            {
                continue;
            }

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

void RosterPage::printRosters()
{
    if (hasUnsavedChanges() && !saveChanges())
    {
        return;
    }

    RosterPrintDialog dialog(
        m_services,
        m_classroom.id,
        RosterTemplatePrintService::Scope::CurrentClass,
        this
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const RosterTemplatePrintService::Result result =
        RosterTemplatePrintService::printRosters(
            {
                this,
                m_services,
                dialog.templatePath(),
                m_classroom.id,
                dialog.selectedScope(),
                dialog.selectedClassIds()
            }
            );

    if (result.status == RosterTemplatePrintService::Status::Failed)
    {
        QMessageBox::warning(
            this,
            tr("Print Rosters"),
            result.message
            );
    }
}

void RosterPage::autosave()
{
    if (!hasUnsavedChanges())
    {
        return;
    }

    saveRosterInternal(false);
}

void RosterPage::updateActions()
{
    if (!m_saveButton || !m_removeStudentButton || !m_removeColumnButton)
    {
        return;
    }

    const bool showSaveButton =
        m_saveMode != SaveMode::Automatic;

    m_saveButton->setVisible(
        showSaveButton
        );

    m_saveButton->setEnabled(
        showSaveButton
        && hasUnsavedChanges()
        && m_classroom.id > 0
        );

    QString reason;

    m_removeColumnButton->setEnabled(
        m_model
        && m_model->canRemoveColumn(
            m_table->currentIndex().column(),
            &reason
            )
        );

    m_removeStudentButton->setEnabled(
        m_model
        && m_model->canRemoveRow(
            m_table->currentIndex().row(),
            &reason
            )
        );

    if (m_printButton)
    {
        m_printButton->setEnabled(
            m_classroom.id > 0
            );
    }
}

void RosterPage::showRosterContextMenu(
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
        const QStringList sourceColumns =
            m_model->columnNames();

        const QStringList sourceRow =
            m_model->rowValues(
                clicked.row()
                );

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

void RosterPage::buildUi()
{
    contentLayout()->setContentsMargins(
        24,
        18,
        24,
        0
        );

    contentLayout()->setSpacing(12);

    auto* headerLayout =
        new QVBoxLayout;

    headerLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );

    headerLayout->setSpacing(2);

    m_titleLabel =
        new QLabel(
            tr("Class Roster"),
            this
            );

    m_titleLabel->setObjectName("pageTitle");
    m_titleLabel->setFont(
        FontManager::getUiFont(
            20,
            QFont::DemiBold
            )
        );

    m_subtitleLabel =
        new QLabel(
            tr("No class selected"),
            this
            );

    m_subtitleLabel->setObjectName("pageSubtitle");
    m_subtitleLabel->setFont(
        FontManager::getUiFont(11)
        );

    headerLayout->addWidget(m_titleLabel);
    headerLayout->addWidget(m_subtitleLabel);

    contentLayout()->addLayout(headerLayout);
    contentLayout()->addSpacing(
        UiConstants::Pages::HeaderContentSpacing
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

    contentLayout()->addWidget(m_table);

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
        tr("Print rosters from a spreadsheet template.")
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

    m_saveButton->setObjectName("primaryButton");

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
        &RosterPage::addColumn
        );

    connect(
        m_removeStudentButton,
        &QPushButton::clicked,
        this,
        &RosterPage::removeStudent
        );

    connect(
        m_removeColumnButton,
        &QPushButton::clicked,
        this,
        &RosterPage::removeColumn
        );

    connect(
        m_saveButton,
        &QPushButton::clicked,
        this,
        &RosterPage::saveData
        );

    connect(
        m_importButton,
        &QPushButton::clicked,
        this,
        &RosterPage::importScores
        );

    connect(
        m_printButton,
        &QPushButton::clicked,
        this,
        &RosterPage::printRosters
        );

    connect(
        m_table,
        &QWidget::customContextMenuRequested,
        this,
        &RosterPage::showRosterContextMenu
        );

    connect(
        m_table,
        &RosterTableView::rowMoveRequested,
        this,
        &RosterPage::moveStudentRow
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
        &RosterPage::updateActions
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

void RosterPage::updateHeaderText()
{
    m_titleLabel->setText(
        tr("Class Roster")
        );

    if (m_classroom.id <= 0)
    {
        m_subtitleLabel->setText(
            tr("No class selected")
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

    const QString className =
        m_classroom.name.trimmed().isEmpty()
            ? tr("Class %1").arg(m_classroom.id)
            : m_classroom.name.trimmed();

    m_subtitleLabel->setText(className);
}

void RosterPage::handleNameCellChanged(
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

void RosterPage::scheduleAutosave()
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

void RosterPage::resolveDuplicateName(
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

void RosterPage::selectRosterCell(
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

bool RosterPage::removeRosterRow(
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

void RosterPage::transferRosterRow(
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

Roster RosterPage::currentRosterForSave() const
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

Roster RosterPage::rosterWithRowRemoved(
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

QVector<int> RosterPage::normalizedColumnWidths(
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

QString RosterPage::rosterRowLabel(
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
