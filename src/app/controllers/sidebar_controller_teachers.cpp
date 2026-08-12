#include "sidebar_controller_p.h"

#include "app/services/feature_services.h"

using namespace SidebarControllerPrivate;

Teacher SidebarController::getTeacherById(int teacherId) const
{
    auto* teachers =
        openTeacherService(m_services);

    return teachers
        ? teachers->teacher(teacherId)
        : Teacher();
}

void SidebarController::addTeacher()
{
    auto* teachers =
        openTeacherService(m_services);

    if (!teachers)
    {
        return;
    }

    Teacher newTeacher;

    int teacherId =
        teachers->create(
            newTeacher
            );

    refreshTeacherSidebar();

    Teacher teacher =
        teachers->teacher(
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
    int teacherId =
        m_sidebar->getSelectedTeacherId();

    if (teacherId <= 0)
    {
        teacherId =
            promptForTeacherToDelete();

        if (teacherId <= 0)
        {
            return;
        }
    }

    auto* teachers =
        openTeacherService(m_services);

    if (!teachers)
    {
        return;
    }

    const Teacher teacher =
        teachers->teacher(
            teacherId
            );

    if (teacher.id <= 0)
    {
        return;
    }

    if (!confirmDeleteTeacher(teacher))
    {
        return;
    }

    teachers->remove(
        teacher.id
        );

    refreshTeacherSidebar();
    refreshClassSidebar();
}

