#include "class_info_page.h"

#include "ui/widgets/sections/teacher_info_section.h"
#include "ui/widgets/sections/class_details_section.h"
#include "ui/widgets/sections/class_schedule_section.h"
#include "ui/widgets/sectioncards/class_info_section_card.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "models/class_conflict.h"
#include "models/class_info.h"
#include "models/teacher.h"
#include "services/dataservice.h"
#include "ui/constants/gui_constants.h"
#include "utils/sidebar_node_naming.h"

#include <QFont>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QFrame>
#include <QScrollArea>
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

ClassInfoPage::ClassInfoPage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
{
    Q_ASSERT(m_services);

    buildUi();

    connect(
        m_saveButton,
        &QPushButton::clicked,
        this,
        &ClassInfoPage::saveData
        );

    connect(
        m_detailsSection,
        &ClassDetailsSection::dataChanged,
        this,
        &ClassInfoPage::markDirty
        );

    connect(
        m_teacherSection,
        &TeacherInfoSection::dataChanged,
        this,
        &ClassInfoPage::markDirty
        );

    connect(
        m_scheduleSection,
        &ClassScheduleSection::dataChanged,
        this,
        &ClassInfoPage::markDirty
        );
}

void ClassInfoPage::buildUi()
{
    contentLayout()->setContentsMargins(
        0,
        0,
        0,
        0
        );

    contentLayout()->setSpacing(
        0
        );

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
        UiConstants::ClassInfo::Page::ContentMargin,
        UiConstants::ClassInfo::Page::ContentMargin,
        UiConstants::ClassInfo::Page::ContentMargin,
        UiConstants::ClassInfo::Page::ContentMargin
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

    m_titleLabel = new QLabel(
        tr("Class Information")
        );
    m_titleLabel->setObjectName("pageTitle");
    m_titleLabel->setFont(
        FontManager::getUiFont(
            24,
            QFont::Bold
            )
        );

    m_subtitleLabel = new QLabel(
        tr("View and manage details for this class.")
        );
    m_subtitleLabel->setObjectName("pageSubtitle");
    m_subtitleLabel->setFont(
        FontManager::getUiFont(11)
        );

    m_scrollContentLayout->addWidget(m_titleLabel);
    m_scrollContentLayout->addWidget(m_subtitleLabel);

    auto* teacherCard =
        addSectionCard(
            m_scrollContentLayout,
            tr("Korean Teacher"),
            m_scrollContent
            );

    m_teacherSection =
        new TeacherInfoSection(teacherCard);

    teacherCard->contentLayout()->addWidget(
        m_teacherSection
        );

    auto* detailsCard =
        addSectionCard(
            m_scrollContentLayout,
            tr("Class Details"),
            m_scrollContent
            );

    m_detailsSection =
        new ClassDetailsSection(
            m_services,
            detailsCard
            );

    detailsCard->contentLayout()->addWidget(
        m_detailsSection
        );

    m_scheduleSection =
        new ClassScheduleSection(this);

    m_scrollContentLayout->addWidget(
        m_scheduleSection
        );

    updateScrollContentMinimumWidth();

    m_saveButton =
        new QPushButton(
            tr("Save Changes")
            );

    m_saveButton->setEnabled(false);

    bottomLayout()->addStretch();
    bottomLayout()->addWidget(
        m_saveButton
        );
}

void ClassInfoPage::updateScrollContentMinimumWidth()
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

void ClassInfoPage::markDirty()
{
    if (m_loading)
    {
        return;
    }

    m_dirty = true;

    m_saveButton->setEnabled(true);
    m_saveButton->setText(
        tr("Save Changes *")
        );
}

void ClassInfoPage::clearDirty()
{
    m_dirty = false;

    m_saveButton->setEnabled(false);
    m_saveButton->setText(
        tr("Save Changes")
        );
}

void ClassInfoPage::loadClass(
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
        info.fontColor
        );

    m_scheduleSection->loadSchedules(
        info.classTimes,
        info.intensiveTimes
        );

    updateScrollContentMinimumWidth();

    m_loading = false;

    clearDirty();
}

void ClassInfoPage::updateTitle(
    const ClassInfo& info
    )
{
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

    m_titleLabel->setText(
        tr("Class Information for %1")
            .arg(displayName)
        );
}

void ClassInfoPage::saveData()
{
    if (m_classroom.id < 0)
    {
        return;
    }

    auto* dataService =
        m_services
            ->dataService();

    const QString currentNotes =
        dataService
            ->loadClassInfo(
                m_classroom.id
                )
            .notes;

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
        currentNotes;

    if (
        showScheduleConflicts(
            info.classTimes,
            ScheduleType::Regular,
            tr("Regular Schedule Conflicts")
            )
        )
    {
        return;
    }

    if (
        showScheduleConflicts(
            info.intensiveTimes,
            ScheduleType::Intensive,
            tr("Intensive Schedule Conflicts")
            )
        )
    {
        return;
    }

    const bool saved =
        dataService->saveClassInfo(info);

    if (!saved)
    {
        m_dirty = true;

        m_saveButton->setEnabled(true);
        m_saveButton->setText(
            tr("Save Changes *")
            );

        QMessageBox::warning(
            this,
            tr("Save Class Information"),
            tr("Class information could not be saved.")
            );

        return;
    }

    clearDirty();

    updateTitle(info);

    emit classInfoSaved(
        m_classroom.id
        );
}

void ClassInfoPage::refresh()
{
    BasePage::refresh();

    /*
    if (m_teacherSection)
    {
        m_teacherSection->refresh();
    }
    */
}

bool ClassInfoPage::showScheduleConflicts(
    const QList<ClassTime>& times,
    ScheduleType type,
    const QString& title
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
