#include "sidebar_controller.h"

#include "core/application_services.h"
#include "services/dataservice.h"

#include "models/class_info.h"
#include "models/classroom.h"
#include "models/teacher.h"

#include "ui/actions/action_registry.h"
#include "ui/pages/pagemanager.h"
#include "ui/pages/class/class_info_page.h"
#include "ui/pages/teacher/teacher_info_page.h"
#include "ui/widgets/sidebar/sidebar.h"

#include "utils/sidebar_node_naming.h"

#include <QMessageBox>

SidebarController::SidebarController(
    ApplicationServices* services,
    Sidebar* sidebar,
    PageManager* pages,
    QObject* parent
    )
    : QObject(parent)
    , m_services(services)
    , m_sidebar(sidebar)
    , m_pages(pages)
{
}

void SidebarController::connectActions(ActionRegistry& actions)
{
    connect(
        m_sidebar,
        &Sidebar::addClassRequested,
        this,
        &SidebarController::addClass
        );

    connect(
        m_sidebar,
        &Sidebar::deleteClassRequested,
        this,
        &SidebarController::deleteClass
        );

    connect(
        m_sidebar,
        &Sidebar::addTeacherRequested,
        this,
        &SidebarController::addTeacher
        );

    connect(
        m_sidebar,
        &Sidebar::deleteTeacherRequested,
        this,
        &SidebarController::deleteTeacher
        );
}

void SidebarController::refreshClassSidebar()
{
    auto* ds = m_services->dataService();

    auto classes = ds->getClasses();

    m_sidebar->clearClasses();

    for (const auto& classroom : classes)
    {
        auto classInfo =
            ds->loadClassInfo(
                classroom.id
                );

        Teacher teacher;

        if (classInfo.teacherId > 0)
        {
            teacher =
                ds->getTeacher(
                    classInfo.teacherId
                    );
        }

        QString displayName =
            SidebarNodeNaming::formatClassDisplayName(
                classInfo,
                teacher
                );

        m_sidebar->addClassNode(
            displayName,
            classroom.id
            );
    }
}

void SidebarController::refreshTeacherSidebar()
{
    auto* ds = m_services->dataService();

    const auto teachers =
        ds->getAllTeachers();

    m_sidebar->clearTeachers();

    for (const auto& teacher : teachers)
    {
        const QString displayName =
            SidebarNodeNaming::formatTeacherDisplayName(
                teacher
                );

        m_sidebar->addTeacherNode(
            displayName,
            teacher.id
            );
    }
}

Classroom SidebarController::getClassById(int classId) const
{
    return m_services
        ->dataService()
        ->getClassById(classId);
}

Classroom SidebarController::getSelectedClass() const
{
    const int classId =
        m_sidebar->getSelectedClassId();

    if (classId <= 0)
    {
        return {};
    }

    return getClassById(classId);
}

void SidebarController::addClass()
{
    auto* ds = m_services->dataService();

    int classId =
        ds->createClass("");

    refreshClassSidebar();

    Classroom classroom =
        ds->getClassById(
            classId
            );

    if (classroom.id == 0)
    {
        return;
    }

    m_pages->classInfoPage()->loadClass(
        classroom
        );

    m_pages->showPage(
        PageType::ClassInfo
        );
}

void SidebarController::deleteClass()
{
    const int classId =
        m_sidebar->getSelectedClassId();

    if (classId <= 0)
    {
        QMessageBox::warning(
            nullptr,
            "Delete Class",
            "No class selected."
            );
        return;
    }

    auto* ds = m_services->dataService();

    const Classroom classroom =
        ds->getClassById(
            classId
            );

    const auto result =
        QMessageBox::question(
            nullptr,
            "Delete Class",
            QString("Delete '%1'?")
                .arg(classroom.name)
            );

    if (result != QMessageBox::Yes)
    {
        return;
    }

    ds->deleteClass(
        classroom.id
        );

    refreshClassSidebar();
}

Teacher SidebarController::getTeacherById(int teacherId) const
{
    return m_services
        ->dataService()
        ->getTeacher(teacherId);
}

void SidebarController::addTeacher()
{
    auto* ds = m_services->dataService();

    Teacher newTeacher;

    int teacherId =
        ds->createTeacher(
            newTeacher
            );

    refreshTeacherSidebar();

    Teacher teacher =
        ds->getTeacher(
            teacherId
            );

    if (teacher.id == 0)
    {
        return;
    }

    m_sidebar->selectTeacher(
        teacherId
        );

    m_pages->teacherPage()->loadTeacher(
        teacher
        );

    m_pages->showPage(
        PageType::TeacherInfo
        );
}

void SidebarController::deleteTeacher()
{
    const int teacherId =
        m_sidebar->getSelectedTeacherId();

    if (teacherId <= 0)
    {
        QMessageBox::warning(
            nullptr,
            "Delete Teacher",
            "No teacher selected."
            );
        return;
    }

    auto* ds = m_services->dataService();

    const Teacher teacher =
        ds->getTeacher(
            teacherId
            );

    if (teacher.id <= 0)
    {
        return;
    }

    const QString displayName =
        SidebarNodeNaming::formatTeacherDisplayName(
            teacher
            );

    const auto result =
        QMessageBox::question(
            nullptr,
            "Delete Teacher",
            QString("Delete '%1'?")
                .arg(displayName)
            );

    if (result != QMessageBox::Yes)
    {
        return;
    }

    ds->deleteTeacher(
        teacher.id
        );

    refreshTeacherSidebar();
}