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
#include "features/roster/ui/roster_table_view.h"
#include "ui/shared/constants/gui_constants.h"

#include <QAbstractItemModel>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{

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

void RosterPage::retranslateUi()
{
    updateHeaderText();

    if (m_importButton)
    {
        m_importButton->setText(tr("Import Scores"));
        m_importButton->setToolTip(
            tr("Import final grades from speaking evaluations.")
            );
    }

    if (m_printButton)
    {
        m_printButton->setText(tr("Print Rosters"));
        m_printButton->setToolTip(tr("Print rosters as an A4 PDF."));
    }

    if (m_addColumnButton)
    {
        m_addColumnButton->setText(tr("Add Column"));
    }

    if (m_removeStudentButton)
    {
        m_removeStudentButton->setText(tr("Remove Student"));
    }

    if (m_removeColumnButton)
    {
        m_removeColumnButton->setText(tr("Remove Column"));
    }

    if (m_saveButton)
    {
        m_saveButton->setText(tr("Save Changes"));
    }

    updateActions();
}

void RosterPage::updateActions()
{
    if (!m_saveButton || !m_removeStudentButton || !m_removeColumnButton)
    {
        return;
    }

    const bool showSaveButton =
        m_saveMode != SaveMode::Automatic;

    m_saveButton->setVisible(showSaveButton);
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
        m_printButton->setEnabled(m_classroom.id > 0);
    }
}

void RosterPage::buildUi()
{
    contentLayout()->setContentsMargins(24, 18, 24, 0);
    contentLayout()->setSpacing(12);

    auto* headerLayout = new QVBoxLayout;
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(2);

    m_titleLabel = new QLabel(tr("Class Roster"), this);
    m_titleLabel->setObjectName("pageTitle");
    m_titleLabel->setFont(FontManager::getUiFont(20, QFont::DemiBold));

    m_subtitleLabel = new QLabel(tr("No class selected"), this);
    m_subtitleLabel->setObjectName("pageSubtitle");
    m_subtitleLabel->setFont(FontManager::getUiFont(11));

    headerLayout->addWidget(m_titleLabel);
    headerLayout->addWidget(m_subtitleLabel);
    contentLayout()->addLayout(headerLayout);
    contentLayout()->addSpacing(UiConstants::Pages::HeaderContentSpacing);

    m_model = new RosterModel(this);
    m_layoutController = new RosterColumnLayoutController(this);
    m_table = new RosterTableView(this);
    m_header = new RosterHeaderView(Qt::Horizontal, m_table);
    m_header->setLayoutController(m_layoutController);
    m_table->setHorizontalHeader(m_header);
    m_table->setModel(m_model);
    m_table->setLayoutController(m_layoutController);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_layoutController->attach(m_table, m_model);

    m_delegate = new RosterItemDelegate(m_layoutController, this);
    m_table->setItemDelegate(m_delegate);
    m_table->verticalHeader()->setDefaultSectionSize(RosterUi::RowHeight);
    m_layoutController->applyWidths({});
    contentLayout()->addWidget(m_table);

    m_importButton = new TextFitPushButton(tr("Import Scores"), this);
    m_importButton->setEnabled(true);
    m_importButton->setToolTip(
        tr("Import final grades from speaking evaluations.")
        );
    m_printButton = new TextFitPushButton(tr("Print Rosters"), this);
    m_printButton->setToolTip(tr("Print rosters as an A4 PDF."));
    m_addColumnButton = new TextFitPushButton(tr("Add Column"), this);
    m_removeStudentButton = new TextFitPushButton(tr("Remove Student"), this);
    m_removeColumnButton = new TextFitPushButton(tr("Remove Column"), this);
    m_saveButton = new TextFitPushButton(tr("Save Changes"), this);
    m_saveButton->setObjectName("primaryButton");

    bottomLayout()->addWidget(m_importButton);
    bottomLayout()->addWidget(m_printButton);
    bottomLayout()->addStretch();
    bottomLayout()->addWidget(m_removeStudentButton);
    bottomLayout()->addWidget(m_addColumnButton);
    bottomLayout()->addWidget(m_removeColumnButton);
    bottomLayout()->addWidget(m_saveButton);

    connect(m_addColumnButton, &QPushButton::clicked, this, &RosterPage::addColumn);
    connect(m_removeStudentButton, &QPushButton::clicked, this, &RosterPage::removeStudent);
    connect(m_removeColumnButton, &QPushButton::clicked, this, &RosterPage::removeColumn);
    connect(m_saveButton, &QPushButton::clicked, this, &RosterPage::saveData);
    connect(m_importButton, &QPushButton::clicked, this, &RosterPage::importScores);
    connect(m_printButton, &QPushButton::clicked, this, &RosterPage::printRosters);
    connect(
        m_table,
        &QWidget::customContextMenuRequested,
        this,
        &RosterPage::showRosterContextMenu
        );
    connect(m_table, &RosterTableView::rowMoveRequested, this, &RosterPage::moveStudentRow);
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
        [this](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>&)
        {
            handleNameCellChanged(topLeft, bottomRight);
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

            m_layoutController->handleSectionResized(logicalIndex);
            m_widthsDirty = true;
            scheduleAutosave();
            updateActions();
        }
        );

    updateActions();
}

void RosterPage::updateHeaderText()
{
    m_titleLabel->setText(tr("Class Roster"));

    if (m_classroom.id <= 0)
    {
        m_subtitleLabel->setText(tr("No class selected"));
        return;
    }

    const QString sidebarName = sidebarClassDisplayName(
        m_services ? m_services->dataService() : nullptr,
        m_classroom.id
        );

    if (!sidebarName.isEmpty())
    {
        m_subtitleLabel->setText(sidebarName);
        return;
    }

    const QString className =
        m_classroom.name.trimmed().isEmpty()
            ? tr("Class %1").arg(m_classroom.id)
            : m_classroom.name.trimmed();

    m_subtitleLabel->setText(className);
}
