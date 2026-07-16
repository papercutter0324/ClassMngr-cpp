#include "class_details_page.h"

#include "ui/shared/widgets/text_fit_push_button.h"

#include "ui/shared/widgets/sections/teacher_info_section.h"
#include "ui/shared/widgets/sections/class_details_section.h"
#include "ui/shared/widgets/sections/class_schedule_section.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "domain/models/class_conflict.h"
#include "domain/models/class_info.h"
#include "domain/models/teacher.h"
#include "data/data_service.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"
#include "core/utils/sidebar_node_naming.h"

#include <QFont>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QFrame>
#include <QScrollArea>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QtAssert>

namespace
{
constexpr int AutosaveDelayMs = 750;

SectionCard* addSectionCard(
    QVBoxLayout* layout,
    const QString& title,
    QWidget* parent
    )
{
    auto* card = new SectionCard(title, parent);

    layout->addWidget(
        card,
        0,
        Qt::AlignTop
        );

    return card;
}
}

ClassDetailsPage::ClassDetailsPage(
    ApplicationServices* services,
    bool embedded,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
    , m_embedded(embedded)
{
    Q_ASSERT(m_services);

    setProperty("role", UiRoles::ClassInfo);

    if (m_embedded)
    {
        setPageLayoutMargins({});
    }

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
        &ClassDetailsPage::autosave
        );

    connect(
        m_saveButton,
        &QPushButton::clicked,
        this,
        &ClassDetailsPage::saveData
        );

    connect(
        m_detailsSection,
        &ClassDetailsSection::dataChanged,
        this,
        &ClassDetailsPage::markDirty
        );

    connect(
        m_teacherSection,
        &TeacherInfoSection::dataChanged,
        this,
        &ClassDetailsPage::markDirty
        );

    connect(
        m_scheduleSection,
        &ClassScheduleSection::dataChanged,
        this,
        &ClassDetailsPage::markDirty
        );
}

void ClassDetailsPage::buildUi()
{
    contentLayout()->setContentsMargins(
        m_embedded ? 0 : UiConstants::Pages::Margin,
        m_embedded ? 0 : 18,
        m_embedded ? 0 : UiConstants::Pages::Margin,
        0
        );

    contentLayout()->setSpacing(
        12
        );

    m_titleLabel =
        new QLabel(
            tr("Class Information"),
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

    if (m_embedded)
    {
        m_titleLabel->hide();
        m_subtitleLabel->hide();
    }
    else
    {
        auto* headerLayout =
            new QVBoxLayout;

        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(2);
        headerLayout->addWidget(m_titleLabel);
        headerLayout->addWidget(m_subtitleLabel);

        contentLayout()->addLayout(headerLayout);
        contentLayout()->addSpacing(
            UiConstants::Pages::HeaderContentSpacing
            );
    }

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAsNeeded
        );
    m_scrollArea->setVerticalScrollBarPolicy(
        Qt::ScrollBarAsNeeded
        );

    m_scrollContent = new QWidget(m_scrollArea);
    m_scrollContentLayout = new QVBoxLayout(m_scrollContent);
    m_scrollContentLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );
    m_scrollContentLayout->setSpacing(
        UiConstants::ClassInfo::Page::ContentSpacing
        );
    m_scrollContentLayout->setAlignment(
        Qt::AlignTop
        );

    m_scrollArea->setWidget(
        m_scrollContent
        );

    contentLayout()->addWidget(
        m_scrollArea
        );

    m_teacherCard =
        addSectionCard(
            m_scrollContentLayout,
            tr("Korean Teacher"),
            m_scrollContent
            );

    m_teacherSection =
        new TeacherInfoSection(m_teacherCard);

    m_teacherCard->contentLayout()->addWidget(
        m_teacherSection
        );

    m_detailsCard =
        addSectionCard(
            m_scrollContentLayout,
            tr("Class Details"),
            m_scrollContent
            );

    m_detailsSection =
        new ClassDetailsSection(
            m_services,
            m_detailsCard
            );

    m_detailsCard->contentLayout()->addWidget(
        m_detailsSection
        );

    m_scheduleCard =
        addSectionCard(
            m_scrollContentLayout,
            tr("Class Times"),
            m_scrollContent
            );

    m_scheduleSection =
        new ClassScheduleSection(m_scheduleCard);

    m_scheduleCard->contentLayout()->addWidget(
        m_scheduleSection
        );

    updateScrollContentMinimumWidth();

    m_saveButton =
        new TextFitPushButton(
            tr("Save Changes")
            );

    m_saveButton->setEnabled(false);

    bottomLayout()->addStretch();
    bottomLayout()->addWidget(
        m_saveButton
        );

    updateActions();
}

void ClassDetailsPage::updateScrollContentMinimumWidth()
{
    if (
        !m_scrollContent
        || !m_scrollContentLayout
        )
    {
        return;
    }

    m_scrollContentLayout->activate();

    m_scrollContent->setMinimumWidth(
        m_scrollContent->minimumSizeHint().width()
        );
}

void ClassDetailsPage::markDirty()
{
    if (m_loading)
    {
        return;
    }

    m_dirty = true;

    updateActions();

    if (
        m_autosaveTimer
        && m_saveMode == SaveMode::Automatic
        )
    {
        m_autosaveTimer->start();
    }
}

void ClassDetailsPage::clearDirty()
{
    m_dirty = false;

    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }
    updateActions();
}

void ClassDetailsPage::loadClass(
    const Classroom& classroom
    )
{
    m_loading = true;

    refresh();

    m_classroom = classroom;

    auto* dataService =
        m_services
            ->dataService();

    m_teacherSection->setTeachers(
        dataService->getAllTeachers()
        );

    const ClassInfo info =
        dataService->loadClassInfo(
            classroom.id
            );

    updateTitle(info);

    m_teacherSection->selectTeacher(
        info.teacherId
        );

    m_detailsSection->loadInfo(
        info.classGrade,
        info.classLevel,
        info.readingBook,
        info.essayBook,
        info.classColor,
        info.fontColor,
        dataService->getRosterStudentCount(
            classroom.id
            )
        );

    m_scheduleSection->loadSchedules(
        info.classTimes,
        info.intensiveTimes
        );

    updateScrollContentMinimumWidth();

    m_loading = false;

    clearDirty();
}

void ClassDetailsPage::updateTitle(
    const ClassInfo& info
    )
{
    m_titleLabel->setText(
        tr("Class Information")
        );

    Teacher teacher;

    if (info.teacherId > 0)
    {
        teacher =
            m_services
                ->dataService()
                ->getTeacher(
                    info.teacherId
                    );
    }

    const QString displayName =
        SidebarNodeNaming::formatClassDisplayName(
            info,
            teacher
            );

    const QString fallbackName =
        m_classroom.name.trimmed().isEmpty()
            ? tr("Class %1").arg(m_classroom.id)
            : m_classroom.name.trimmed();

    m_subtitleLabel->setText(
        displayName.trimmed().isEmpty()
            ? fallbackName
            : displayName
        );
}

void ClassDetailsPage::saveData()
{
    saveClassInfoInternal(true);
}

bool ClassDetailsPage::saveChanges()
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    if (!hasUnsavedChanges())
    {
        return true;
    }

    return saveClassInfoInternal(true);
}

bool ClassDetailsPage::hasUnsavedChanges() const
{
    return m_dirty;
}

void ClassDetailsPage::discardChanges()
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    loadClass(m_classroom);
}

void ClassDetailsPage::setSaveMode(
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

void ClassDetailsPage::autosave()
{
    if (!hasUnsavedChanges())
    {
        return;
    }

    saveClassInfoInternal(false);
}

void ClassDetailsPage::updateActions()
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
        && m_classroom.id >= 0
        );

    m_saveButton->setText(
        m_dirty
            ? tr("Save Changes *")
            : tr("Save Changes")
        );
}

bool ClassDetailsPage::saveClassInfoInternal(
    bool showMessages
    )
{
    if (m_classroom.id < 0)
    {
        return true;
    }

    auto* dataService =
        m_services
            ->dataService();

    const ClassInfo currentInfo =
        dataService
            ->loadClassInfo(
                m_classroom.id
                );

    ClassInfo info;
    info.classId =
        m_classroom.id;

    info.teacherId =
        m_teacherSection->teacherId();

    info.classGrade =
        m_detailsSection->grade();

    info.classLevel =
        m_detailsSection->level();

    info.readingBook =
        m_detailsSection->readingBook();

    info.essayBook =
        m_detailsSection->essayBook();

    info.classColor =
        m_detailsSection->classColor();

    info.fontColor =
        m_detailsSection->fontColor();

    info.classTimes =
        m_scheduleSection->regularTimes();

    info.intensiveTimes =
        m_scheduleSection->intensiveTimes();

    info.notes =
        currentInfo.notes;

    info.timeFillerActivities =
        currentInfo.timeFillerActivities;

    if (
        showScheduleConflicts(
            info.classTimes,
            ScheduleType::Regular,
            tr("Regular Schedule Conflicts"),
            showMessages
            )
        )
    {
        return false;
    }

    if (
        showScheduleConflicts(
            info.intensiveTimes,
            ScheduleType::Intensive,
            tr("Intensive Schedule Conflicts"),
            showMessages
            )
        )
    {
        return false;
    }

    const bool saved =
        dataService->saveClassInfo(info);

    if (!saved)
    {
        m_dirty = true;

        updateActions();

        if (showMessages)
        {
            QMessageBox::warning(
                this,
                tr("Save Class Information"),
                tr("Class information could not be saved.")
                );
        }

        return false;
    }

    clearDirty();

    updateTitle(info);

    emit classInfoSaved(
        m_classroom.id
        );

    return true;
}

void ClassDetailsPage::refresh()
{
    BasePage::refresh();

    /*
    if (m_teacherSection)
    {
        m_teacherSection->refresh();
    }
    */
}

void ClassDetailsPage::retranslateUi()
{
    if (
        m_services
        && m_services->dataService()
        && m_classroom.id >= 0
        )
    {
        updateTitle(
            m_services
                ->dataService()
                ->loadClassInfo(m_classroom.id)
            );
    }
    else
    {
        m_titleLabel->setText(
            tr("Class Information")
            );

        m_subtitleLabel->setText(
            tr("No class selected")
            );
    }

    if (m_teacherCard)
    {
        m_teacherCard->setTitle(
            tr("Korean Teacher")
            );
    }

    if (m_detailsCard)
    {
        m_detailsCard->setTitle(
            tr("Class Details")
            );
    }

    if (m_scheduleCard)
    {
        m_scheduleCard->setTitle(
            tr("Class Times")
            );
    }

    if (m_teacherSection)
    {
        m_teacherSection->retranslateUi();
    }

    if (m_detailsSection)
    {
        m_detailsSection->retranslateUi();
    }

    if (m_scheduleSection)
    {
        m_scheduleSection->retranslateUi();
    }

    updateScrollContentMinimumWidth();
    updateActions();
}

bool ClassDetailsPage::showScheduleConflicts(
    const QList<ClassTime>& times,
    ScheduleType type,
    const QString& title,
    bool showMessage
    )
{
    const QList<ClassConflict> conflicts =
        m_services
            ->dataService()
            ->getClassTimeConflicts(
                m_classroom.id,
                times,
                type
                );

    if (conflicts.isEmpty())
    {
        return false;
    }

    if (!showMessage)
    {
        return true;
    }

    QStringList details;

    for (const ClassConflict& conflict : conflicts)
    {
        QString conflictingClass =
            conflict.conflictingClassName;

        if (conflictingClass == conflict.className)
        {
            conflictingClass =
                tr("another time in this class");
        }

        details.append(
            tr("%1 %2-%3 conflicts with %4.")
                .arg(conflict.day)
                .arg(conflict.startTime)
                .arg(conflict.endTime)
                .arg(conflictingClass)
            );
    }

    QMessageBox::warning(
        this,
        title,
        tr("Please resolve these schedule conflicts before saving:\n\n%1")
            .arg(details.join('\n'))
        );

    return true;
}
