#include "class_notes_page.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "domain/models/class_info.h"
#include "domain/models/teacher.h"
#include "data/data_service.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"
#include "core/utils/sidebar_node_naming.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QtAssert>

namespace
{
constexpr int AutosaveDelayMs = 750;
}

ClassNotesPage::ClassNotesPage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
{
    Q_ASSERT(m_services);

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
        &ClassNotesPage::autosave
        );
}

void ClassNotesPage::loadClass(
    const Classroom& classroom
    )
{
    m_loading = true;

    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    m_classroom = classroom;

    auto* dataService =
        m_services
            ->dataService();

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

    m_subtitleText =
        SidebarNodeNaming::formatClassDisplayName(
            info,
            teacher
            );

    if (m_subtitleText.trimmed().isEmpty())
    {
        m_subtitleText =
            classroom.name.trimmed().isEmpty()
                ? tr("Class %1").arg(classroom.id)
                : classroom.name.trimmed();
    }

    m_savedNotes =
        info.notes.trimmed();

    m_savedTimeFillerActivities =
        info.timeFillerActivities.trimmed();

    m_notesEdit->setPlainText(
        m_savedNotes
        );

    m_timeFillerActivitiesEdit->setPlainText(
        m_savedTimeFillerActivities
        );

    updateHeaderText();

    m_loading = false;
    clearDirty();
}

void ClassNotesPage::saveData()
{
    saveClassNotesInternal(true);
}

bool ClassNotesPage::saveChanges()
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    return saveClassNotesInternal(true);
}

bool ClassNotesPage::hasUnsavedChanges() const
{
    return m_dirty;
}

void ClassNotesPage::discardChanges()
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    loadClass(m_classroom);
}

void ClassNotesPage::setSaveMode(
    SaveMode mode
    )
{
    m_saveMode =
        mode;

    updateActions();

    if (!m_autosaveTimer)
    {
        return;
    }

    if (
        m_saveMode == SaveMode::Automatic
        && hasUnsavedChanges()
        )
    {
        m_autosaveTimer->start();
    }
    else
    {
        m_autosaveTimer->stop();
    }
}

void ClassNotesPage::autosave()
{
    if (!hasUnsavedChanges())
    {
        return;
    }

    saveClassNotesInternal(false);
}

bool ClassNotesPage::saveClassNotesInternal(
    bool showErrorMessage
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

    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    const QString notes =
        m_notesEdit
            ->toPlainText()
            .trimmed();

    const QString timeFillerActivities =
        m_timeFillerActivitiesEdit
            ->toPlainText()
            .trimmed();

    if (
        !m_services
            ->dataService()
            ->saveClassNotes(
                m_classroom.id,
                notes,
                timeFillerActivities
                )
        )
    {
        if (showErrorMessage)
        {
            QMessageBox::warning(
                this,
                tr("Save Class Notes"),
                tr("Class notes could not be saved.")
                );
        }

        return false;
    }

    m_savedNotes =
        notes;

    m_savedTimeFillerActivities =
        timeFillerActivities;

    clearDirty();
    return true;
}

void ClassNotesPage::refresh()
{
    BasePage::refresh();
}

void ClassNotesPage::retranslateUi()
{
    updateHeaderText();

    if (m_saveButton)
    {
        m_saveButton->setText(
            tr("Save Changes")
            );
    }

    if (m_notesCard)
    {
        m_notesCard->setTitle(
            tr("Notes")
            );
    }

    if (m_timeFillerActivitiesLabel)
    {
        m_timeFillerActivitiesLabel->setText(
            tr("Time Filler Activities")
            );
    }

    updateActions();
}

void ClassNotesPage::markDirty()
{
    if (m_loading)
    {
        return;
    }

    m_dirty =
        m_notesEdit
        && m_timeFillerActivitiesEdit
        && (
            m_notesEdit->toPlainText().trimmed() != m_savedNotes
            || m_timeFillerActivitiesEdit->toPlainText().trimmed()
                != m_savedTimeFillerActivities
            );

    updateActions();

    if (!m_autosaveTimer)
    {
        return;
    }

    if (
        m_dirty
        && m_saveMode == SaveMode::Automatic
        )
    {
        m_autosaveTimer->start();
    }
    else
    {
        m_autosaveTimer->stop();
    }
}

void ClassNotesPage::buildUi()
{
    contentLayout()->setContentsMargins(
        UiConstants::Pages::Margin,
        18,
        UiConstants::Pages::Margin,
        0
        );

    contentLayout()->setSpacing(12);

    auto* headerLayout =
        new QVBoxLayout;

    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(2);

    m_titleLabel =
        new QLabel(
            tr("Class Notes"),
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

    m_notesCard =
        new SectionCard(
            tr("Notes"),
            this
            );

    m_notesEdit =
        new QTextEdit(m_notesCard);

    m_notesEdit->setMinimumHeight(
        UiConstants::Editors::NotesMinimumHeight
        );

    m_notesCard->contentLayout()->addWidget(
        m_notesEdit
        );

    m_notesCard->contentLayout()->addSpacing(
        UiConstants::Cards::Spacing
        );

    m_timeFillerActivitiesLabel =
        new QLabel(
            tr("Time Filler Activities"),
            m_notesCard
            );

    m_timeFillerActivitiesLabel->setContentsMargins(
        UiConstants::Forms::LabelIndent,
        0,
        0,
        0
        );

    m_notesCard->contentLayout()->addWidget(
        m_timeFillerActivitiesLabel
        );

    m_timeFillerActivitiesEdit =
        new QTextEdit(m_notesCard);

    m_timeFillerActivitiesEdit->setMinimumHeight(
        UiConstants::Editors::NotesMinimumHeight
        );

    m_notesCard->contentLayout()->addWidget(
        m_timeFillerActivitiesEdit
        );

    contentLayout()->addWidget(m_notesCard);
    contentLayout()->addStretch();

    m_saveButton =
        new QPushButton(
            tr("Save Changes"),
            this
            );

    m_saveButton->setObjectName("primaryButton");
    m_saveButton->setEnabled(false);

    bottomLayout()->addStretch();
    bottomLayout()->addWidget(m_saveButton);

    connect(
        m_notesEdit,
        &QTextEdit::textChanged,
        this,
        &ClassNotesPage::markDirty
        );

    connect(
        m_timeFillerActivitiesEdit,
        &QTextEdit::textChanged,
        this,
        &ClassNotesPage::markDirty
        );

    connect(
        m_saveButton,
        &QPushButton::clicked,
        this,
        &ClassNotesPage::saveData
        );

    updateActions();
}

void ClassNotesPage::updateHeaderText()
{
    m_titleLabel->setText(
        tr("Class Notes")
        );

    m_subtitleLabel->setText(
        m_subtitleText.trimmed().isEmpty()
            ? tr("No class selected")
            : m_subtitleText
        );
}

void ClassNotesPage::clearDirty()
{
    m_dirty = false;

    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    updateActions();
}

void ClassNotesPage::updateActions()
{
    if (!m_saveButton)
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
        && m_dirty
        && m_classroom.id > 0
        );
}
