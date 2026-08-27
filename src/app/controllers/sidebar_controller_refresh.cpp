#include "sidebar_controller_p.h"

#include "app/services/feature_services.h"
#include "ui/shared/dialogs/user_prompt_service.h"

using namespace SidebarControllerPrivate;

void SidebarController::refreshTeacherSidebar()
{
    if (!m_sidebar)
    {
        return;
    }

    m_sidebar->clearTeachers();

    auto* classes =
        openClassService(m_services);
    auto* teacherService =
        openTeacherService(m_services);

    if (!classes || !teacherService)
    {
        updateActionStates();
        return;
    }

    const Result<QList<Teacher>> teachers =
        teacherService->teachers();
    const Result<QList<ClassTeacherAssignment>> assignments =
        classes->classTeacherAssignments();
    if (!teachers || !assignments)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Load Teachers"),
            tr("Teachers and their classes could not be loaded."),
            !teachers ? teachers.error() : assignments.error()
            );
        updateActionStates();
        return;
    }

    QHash<int, Teacher> teachersById;

    for (const Teacher& teacher : *teachers)
    {
        if (teacher.id > 0)
        {
            teachersById.insert(
                teacher.id,
                teacher
                );
        }
    }

    QSet<int> myTeacherIds;
    QList<Teacher> myTeachers;

    for (const ClassTeacherAssignment& assignment : *assignments)
    {
        const int teacherId =
            assignment.teacherId;

        if (
            teacherId <= 0
            || myTeacherIds.contains(teacherId)
            || !teachersById.contains(teacherId)
            )
        {
            continue;
        }

        myTeacherIds.insert(
            teacherId
            );
        myTeachers.append(
            teachersById.value(teacherId)
            );
    }

    for (const Teacher& teacher : sortedTeachers(myTeachers))
    {
        const QString displayName =
            SidebarNodeNaming::formatTeacherDisplayName(
                teacher
                );

        m_sidebar->addTeacherNode(
            displayName,
            teacher.id,
            true
            );
    }

    for (const Teacher& teacher : sortedTeachers(*teachers))
    {
        const QString displayName =
            SidebarNodeNaming::formatTeacherDisplayName(
                teacher
                );

        m_sidebar->addTeacherNode(
            displayName,
            teacher.id,
            false
            );
    }

    updateActionStates(
        !assignments->isEmpty(),
        !teachers->isEmpty()
        );
}

void SidebarController::refreshAllSidebars()
{
    refreshTeacherSidebar();
}

void SidebarController::handleClassInfoSaved(
    int /*classId*/
    )
{
    updateActionStates();
}

void SidebarController::handleTeacherSaved(
    int teacherId
    )
{
    refreshTeacherSidebar();

    m_sidebar->selectTeacher(
        teacherId
        );
}
