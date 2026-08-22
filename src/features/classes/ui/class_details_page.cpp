#include "class_details_page.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "ui/shared/widgets/text_fit_push_button.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/pages/scrollable_page_body.h"

#include "ui/shared/widgets/sections/teacher_info_section.h"
#include "ui/shared/widgets/sections/class_details_section.h"
#include "ui/shared/widgets/sections/class_schedule_section.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"

#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "domain/models/class_conflict.h"
#include "domain/models/class_info.h"
#include "domain/models/teacher.h"
#include "core/fontmanager.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"
#include "core/utils/sidebar_node_naming.h"

#include <QFont>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>
#include <QtAssert>

namespace
{
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
    , m_autosave(new AutosaveCoordinator(this))
{
    Q_ASSERT(m_services);

    setProperty("role", UiRoles::ClassInfo);

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
            saveClassInfoInternal(interactive);
        }
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
        m_embedded ? 0 : UiConstants::Pages::Margin,
        m_embedded ? 0 : UiConstants::Pages::Margin,
        0
        );

    contentLayout()->setSpacing(
        m_embedded
            ? 12
            : UiConstants::Pages::Spacing
        );

    if (m_embedded)
    {
        m_embeddedHeading = new QLabel(tr("Class Details"), this);
        m_embeddedHeading->setObjectName(
            QStringLiteral("classDetailsHeading")
            );
        m_embeddedHeading->setFont(
            FontManager::getUiFont(18, QFont::DemiBold)
            );
        contentLayout()->addWidget(m_embeddedHeading);
    }

    m_pageHeader = new PageHeader(
        tr("Class Information"),
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

    m_pageBody = new ScrollablePageBody(
        this,
        QMargins(0, 0, 0, 0),
        UiConstants::ClassInfo::Page::ContentSpacing
        );
    m_pageBody->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAsNeeded
        );
    m_pageBody->setVerticalScrollBarPolicy(
        Qt::ScrollBarAsNeeded
        );
    m_scrollContent = m_pageBody->contentWidget();
    m_scrollContentLayout = m_pageBody->contentLayout();

    contentLayout()->addWidget(
        m_pageBody
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
    if (m_autosave->isLoading())
    {
        return;
    }

    m_autosave->markDirty();
}

void ClassDetailsPage::clearDirty()
{
    m_autosave->markClean();
    updateActions();
}

void ClassDetailsPage::loadClass(
    const Classroom& classroom
    )
{
    m_autosave->setLoading(true);

    refresh();

    m_classroom = classroom;

    auto* classService = m_services->classService();
    auto* teacherService = m_services->teacherService();
    auto* rosterService = m_services->rosterService();

    const Result<QList<Teacher>> teachers = teacherService->teachers();
    if (!teachers)
    {
        DialogServices::showWarning(
            this,
            tr("Load Class"),
            tr("Teachers could not be loaded."),
            teachers.error()
            );
        m_autosave->setLoading(false);
        clearDatabaseState();
        return;
    }
    m_teacherSection->setTeachers(*teachers);

    const ClassInfo info =
        classService->classInfo(
            classroom.id
            ).value_or(ClassInfo{});

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
        rosterService->studentCount(
            classroom.id
            ).value_or(0)
        );

    m_scheduleSection->loadSchedules(
        info.classTimes,
        info.intensiveTimes
        );

    updateScrollContentMinimumWidth();

    m_autosave->setLoading(false);

    clearDirty();
}

void ClassDetailsPage::clearDatabaseState()
{
    m_autosave->setLoading(true);

    m_classroom = {};
    m_teacherSection->setTeachers({});
    m_teacherSection->selectTeacher(-1);
    m_detailsSection->loadInfo({}, {}, {}, {}, {}, {}, 0);
    m_scheduleSection->loadSchedules(
        QList<ClassTime>{},
        QList<ClassTime>{}
        );
    updateTitle({});
    m_pageHeader->setSubtitle(
        tr("No class selected")
        );
    updateScrollContentMinimumWidth();

    m_autosave->setLoading(false);
    clearDirty();
}

void ClassDetailsPage::updateTitle(
    const ClassInfo& info
    )
{
    m_pageHeader->setTitle(tr("Class Information"));

    Teacher teacher;

    if (info.teacherId > 0)
    {
        teacher = m_services
            ->teacherService()
            ->teacher(info.teacherId)
            .value_or(Teacher{});
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

    m_pageHeader->setSubtitle(
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
    m_autosave->cancelPendingSave();

    if (!hasUnsavedChanges())
    {
        return true;
    }

    return saveClassInfoInternal(true);
}

bool ClassDetailsPage::hasUnsavedChanges() const
{
    return m_autosave->isDirty();
}

void ClassDetailsPage::discardChanges()
{
    m_autosave->cancelPendingSave();

    loadClass(m_classroom);
}

void ClassDetailsPage::setSaveMode(
    SaveMode mode
    )
{
    m_autosave->setSaveMode(mode);
}

void ClassDetailsPage::updateActions()
{
    m_autosave->setSaveAvailable(m_classroom.id >= 0);
    m_autosave->setSaveMode(m_autosave->saveMode());
}

bool ClassDetailsPage::saveClassInfoInternal(
    bool showMessages
    )
{
    if (m_classroom.id < 0)
    {
        return true;
    }

    auto* classService = m_services->classService();

    const ClassInfo currentInfo =
        classService->classInfo(
                m_classroom.id
                ).value_or(ClassInfo{});

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

    const Status saved =
        classService->saveClassInfo(info);

    if (!saved)
    {
        m_autosave->markDirty(false);

        if (showMessages)
        {
            DialogServices::showWarning(
                this,
                tr("Save Class Information"),
                saved.error()
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
    if (m_embeddedHeading)
    {
        m_embeddedHeading->setText(tr("Class Details"));
    }

    if (
        m_services
        && m_services->classService()
        && m_classroom.id >= 0
        )
    {
        updateTitle(
            m_services
                ->classService()
                ->classInfo(m_classroom.id)
                .value_or(ClassInfo{})
            );
    }
    else
    {
        m_pageHeader->setTitle(tr("Class Information"));
        m_pageHeader->setSubtitle(tr("No class selected"));
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
    const Result<QList<ClassConflict>> loadedConflicts =
        m_services
            ->classService()
            ->conflicts(
                m_classroom.id,
                times,
                type
                );

    if (!loadedConflicts)
    {
        if (showMessage)
        {
            DialogServices::showWarning(
                this,
                title,
                loadedConflicts.error()
                );
        }

        return true;
    }

    const QList<ClassConflict>& conflicts = *loadedConflicts;

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

    DialogServices::showWarning(
        this,
        title,
        tr("Please resolve these schedule conflicts before saving:\n\n%1")
            .arg(details.join('\n'))
        );

    return true;
}
