#include "sidebar_controller_p.h"

#include "app/services/feature_services.h"
#include "ui/shared/dialogs/user_prompt_service.h"

using namespace SidebarControllerPrivate;

Teacher SidebarController::getTeacherById(int teacherId) const
{
    auto* teachers =
        openTeacherService(m_services);

    return teachers
        ? teachers->teacher(teacherId).value_or(Teacher{})
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

    const Result<int> created = teachers->create(newTeacher);
    if (!created)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Add Teacher"),
            tr("The teacher could not be created."),
            created.error()
            );
        return;
    }
    const int teacherId = *created;

    refreshTeacherSidebar();

    const Result<Teacher> teacher =
        teachers->teacher(
            teacherId
            );

    if (!teacher)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Add Teacher"),
            tr("The created teacher could not be loaded."),
            teacher.error()
            );
        return;
    }

    m_sidebar->selectTeacher(
        teacherId
        );

    m_pages->teacherPage()->loadTeacher(
            *teacher
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

    const Result<Teacher> teacher =
        teachers->teacher(
            teacherId
            );

    if (!teacher)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Delete Teacher"),
            tr("The teacher could not be loaded."),
            teacher.error()
            );
        return;
    }

    if (!confirmDeleteTeacher(*teacher))
    {
        return;
    }

    const Status removed = teachers->remove(teacher->id);
    if (!removed)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Delete Teacher"),
            tr("The teacher could not be deleted."),
            removed.error()
            );
        return;
    }

    refreshTeacherSidebar();
    refreshClassSidebar();
}
