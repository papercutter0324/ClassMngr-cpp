#include "sidebar_controller.h"

#include "utils/sidebar_node_naming.h"
#include "ui/pages/pagemanager.h"
#include "ui/pages/teacher/teacher_info_page.h"
#include "ui/pages/class/class_info_page.h"
#include "ui/widgets/sidebar/sidebar.h"
#include "models/teacher.h"
#include "core/application_services.h"
#include "services/dataservice.h"


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