#include "sidebar_controller_p.h"

using namespace SidebarControllerPrivate;

Teacher SidebarController::getTeacherById(int teacherId) const
{
    auto* ds =
        openDataService(m_services);

    return ds
        ? ds->getTeacher(teacherId)
        : Teacher();
}

void SidebarController::addTeacher()
{
    auto* ds =
        openDataService(m_services);

    if (!ds)
    {
        return;
    }

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

    auto* ds =
        openDataService(m_services);

    if (!ds)
    {
        return;
    }

    const Teacher teacher =
        ds->getTeacher(
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

    ds->deleteTeacher(
        teacher.id
        );

    refreshTeacherSidebar();
    refreshClassSidebar();
}

