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
#include <QVBoxLayout>
#include <QtAssert>

ClassNotesPage::ClassNotesPage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
{
    Q_ASSERT(m_services);

    buildUi();
}

void ClassNotesPage::loadClass(
    const Classroom& classroom
    )
{
    m_loading = true;
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
    if (
        !m_services
        || !m_services->dataService()
        || m_classroom.id <= 0
        )
    {
        return;
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
        QMessageBox::warning(
            this,
            tr("Save Class Notes"),
            tr("Class notes could not be saved.")
            );

        return;
    }

    const ClassInfo info =
        m_services
            ->dataService()
            ->loadClassInfo(
                m_classroom.id
                );

    m_loading = true;

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

    m_loading = false;
    clearDirty();
}

void ClassNotesPage::refresh()
{
    BasePage::refresh();
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

    auto* notesCard =
        new SectionCard(
            tr("Notes"),
            this
            );

    m_notesEdit =
        new QTextEdit(notesCard);

    m_notesEdit->setMinimumHeight(
        UiConstants::Editors::NotesMinimumHeight
        );

    notesCard->contentLayout()->addWidget(
        m_notesEdit
        );

    notesCard->contentLayout()->addSpacing(
        UiConstants::Cards::Spacing
        );

    auto* timeFillerActivitiesLabel =
        new QLabel(
            tr("Time Filler Activities"),
            notesCard
            );

    timeFillerActivitiesLabel->setContentsMargins(
        UiConstants::Forms::LabelIndent,
        0,
        0,
        0
        );

    notesCard->contentLayout()->addWidget(
        timeFillerActivitiesLabel
        );

    m_timeFillerActivitiesEdit =
        new QTextEdit(notesCard);

    m_timeFillerActivitiesEdit->setMinimumHeight(
        UiConstants::Editors::NotesMinimumHeight
        );

    notesCard->contentLayout()->addWidget(
        m_timeFillerActivitiesEdit
        );

    contentLayout()->addWidget(notesCard);
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
    updateActions();
}

void ClassNotesPage::updateActions()
{
    if (!m_saveButton)
    {
        return;
    }

    m_saveButton->setEnabled(
        m_dirty
        && m_classroom.id > 0
        );
}
