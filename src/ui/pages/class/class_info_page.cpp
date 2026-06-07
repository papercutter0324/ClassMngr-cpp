#include "class_info_page.h"

#include "ui/widgets/sections/teacher_info_section.h"
#include "ui/widgets/sections/class_details_section.h"
#include "ui/widgets/sections/class_schedule_section.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

ClassInfoPage::ClassInfoPage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
{
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
        m_scheduleSection,
        &ClassScheduleSection::dataChanged,
        this,
        &ClassInfoPage::markDirty
        );
}

void ClassInfoPage::buildUi()
{
    m_titleLabel = new QLabel(
        tr("Class Information")
        );

    m_subtitleLabel = new QLabel(
        tr("View and manage details for this class.")
        );

    contentLayout()->addWidget(m_titleLabel);
    contentLayout()->addWidget(m_subtitleLabel);

    m_teacherSection =
        new TeacherInfoSection(this);

    m_detailsSection =
        new ClassDetailsSection(
            m_services,
            this
            );

    m_scheduleSection =
        new ClassScheduleSection(this);

    contentLayout()->addWidget(
        m_teacherSection
        );

    contentLayout()->addWidget(
        m_detailsSection
        );

    contentLayout()->addWidget(
        m_scheduleSection
        );

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

    m_titleLabel->setText(
        tr("Class Information for %1")
            .arg(classroom.name)
        );

    /*
    auto info =
        m_services
        ->dataService()
        ->loadClassInfo(
            classroom.id
            );

    m_teacherSection->loadInfo(...);

    m_detailsSection->loadInfo(
        info.classGrade,
        info.classLevel,
        info.readingBook,
        info.essayBook,
        info.classColor,
        info.fontColor
        );

    m_scheduleSection->loadSchedules(
        info.times,
        info.intensiveTimes
        );
    */

    m_loading = false;

    clearDirty();
}

void ClassInfoPage::saveData()
{
    if (m_classroom.id < 0)
    {
        return;
    }

    QVariantList regular =
        m_scheduleSection
            ->serializeRegular();

    QVariantList intensive =
        m_scheduleSection
            ->serializeIntensive();

    if (
        showScheduleConflicts(
            regular,
            tr("Regular Schedule Conflicts")
            )
        )
    {
        return;
    }

    if (
        showScheduleConflicts(
            intensive,
            tr("Intensive Schedule Conflicts")
            )
        )
    {
        return;
    }

    /*
    m_services
        ->dataService()
        ->saveClassInfo(
            ...
            );
    */

    clearDirty();
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
    const QVariantList&,
    const QString&
    )
{
    return false;
}

