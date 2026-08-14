#include "class_notes_page.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "ui/shared/widgets/text_fit_push_button.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/pages/page_header.h"

#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "domain/models/class_info.h"
#include "domain/models/teacher.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"
#include "core/utils/sidebar_node_naming.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QtAssert>

ClassNotesPage::ClassNotesPage(
    ApplicationServices* services,
    bool embedded,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
    , m_embedded(embedded)
    , m_autosave(new AutosaveCoordinator(this))
{
    Q_ASSERT(m_services);

    if (m_embedded)
    {
        setPageLayoutMargins({});
    }

    buildUi();
    m_autosave->bindSaveButton(m_saveButton);
    connect(
        m_autosave,
        &AutosaveCoordinator::saveRequested,
        this,
        [this](bool interactive) {
            saveClassNotesInternal(interactive);
        }
        );
    updateActions();
}

void ClassNotesPage::loadClass(
    const Classroom& classroom
    )
{
    m_autosave->setLoading(true);

    m_classroom = classroom;

    auto* classService = m_services->classService();
    auto* teacherService = m_services->teacherService();

    const ClassInfo info =
        classService->classInfo(
            classroom.id
            );

    Teacher teacher;

    if (info.teacherId > 0)
    {
        teacher = teacherService->teacher(info.teacherId)
            .value_or(Teacher{});
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

    m_autosave->setLoading(false);
    clearDirty();
}

void ClassNotesPage::clearDatabaseState()
{
    m_autosave->setLoading(true);

    m_classroom = {};
    m_savedNotes.clear();
    m_savedTimeFillerActivities.clear();
    m_subtitleText.clear();
    m_notesEdit->clear();
    m_timeFillerActivitiesEdit->clear();
    updateHeaderText();

    m_autosave->setLoading(false);
    clearDirty();
}

void ClassNotesPage::saveData()
{
    saveClassNotesInternal(true);
}

bool ClassNotesPage::saveChanges()
{
    m_autosave->cancelPendingSave();

    return saveClassNotesInternal(true);
}

bool ClassNotesPage::hasUnsavedChanges() const
{
    return m_autosave->isDirty();
}

void ClassNotesPage::discardChanges()
{
    m_autosave->cancelPendingSave();

    loadClass(m_classroom);
}

void ClassNotesPage::setSaveMode(
    SaveMode mode
    )
{
    m_autosave->setSaveMode(mode);
}

bool ClassNotesPage::saveClassNotesInternal(
    bool showErrorMessage
    )
{
    if (
        !m_services
        || !m_services->classService()
        || m_classroom.id <= 0
        )
    {
        return false;
    }

    m_autosave->cancelPendingSave();

    const QString notes =
        m_notesEdit
            ->toPlainText()
            .trimmed();

    const QString timeFillerActivities =
        m_timeFillerActivitiesEdit
            ->toPlainText()
            .trimmed();

    const Status saved = m_services->classService()->saveClassNotes(
        m_classroom.id,
        notes,
        timeFillerActivities
        );
    if (!saved)
    {
        if (showErrorMessage)
        {
            DialogServices::showWarning(
                this,
                tr("Save Class Notes"),
                saved.error()
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
    if (m_autosave->isLoading())
    {
        return;
    }

    m_autosave->setDirty(
        m_notesEdit
        && m_timeFillerActivitiesEdit
        && (
            m_notesEdit->toPlainText().trimmed() != m_savedNotes
            || m_timeFillerActivitiesEdit->toPlainText().trimmed()
                != m_savedTimeFillerActivities
            )
        );
}

void ClassNotesPage::buildUi()
{
    contentLayout()->setContentsMargins(
        m_embedded ? 0 : UiConstants::Pages::Margin,
        m_embedded ? 0 : UiConstants::Pages::Margin,
        m_embedded ? 0 : UiConstants::Pages::Margin,
        0
        );

    contentLayout()->setSpacing(
        m_embedded
            ? 12
            : UiConstants::Pages::Spacing
        );

    m_pageHeader = new PageHeader(
        tr("Class Notes"),
        tr("No class selected"),
        this
        );

    if (m_embedded)
    {
        m_pageHeader->hide();
    }
    else
    {
        contentLayout()->addWidget(m_pageHeader);
        contentLayout()->addSpacing(
            UiConstants::Pages::HeaderContentSpacing
            );
    }

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
        new TextFitPushButton(
            tr("Save Changes"),
            this
            );

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

    updateActions();
}

void ClassNotesPage::updateHeaderText()
{
    m_pageHeader->setTitle(tr("Class Notes"));
    m_pageHeader->setSubtitle(
        m_subtitleText.trimmed().isEmpty()
            ? tr("No class selected")
            : m_subtitleText
        );
}

void ClassNotesPage::clearDirty()
{
    m_autosave->markClean();
    updateActions();
}

void ClassNotesPage::updateActions()
{
    m_autosave->setSaveAvailable(m_classroom.id > 0);
    m_autosave->setSaveMode(m_autosave->saveMode());
}
