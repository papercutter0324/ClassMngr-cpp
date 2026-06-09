#include "class_info_page.h"

#include "ui/widgets/sections/teacher_info_section.h"
#include "ui/widgets/sections/class_details_section.h"
#include "ui/widgets/sections/class_schedule_section.h"

#include "core/application_services.h"
#include "models/class_info.h"
#include "services/dataservice.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtAssert>

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

    m_notes =
        info.notes;

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

    m_loading = false;

    clearDirty();
}

void ClassInfoPage::saveData()
{
    if (m_classroom.id < 0)
    {
        return;
    }

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
        m_notes;

    m_services
        ->dataService()
        ->saveClassInfo(info);

    clearDirty();

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
    const QVariantList&,
    const QString&
    )
{
    return false;
}
