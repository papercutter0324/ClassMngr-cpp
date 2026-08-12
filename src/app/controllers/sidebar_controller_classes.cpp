#include "sidebar_controller_p.h"

using namespace SidebarControllerPrivate;

Classroom SidebarController::getClassById(int classId) const
{
    auto* ds =
        openDataService(m_services);

    return ds
        ? ds->getClassById(classId)
        : Classroom();
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
    auto* ds =
        openDataService(m_services);

    if (!ds)
    {
        return;
    }

    if (!m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

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

    m_pages->classesPage()->openClass(
        classroom.id,
        ClassesSection::Details
        );

    m_pages->showPage(
        PageType::Classes
        );

    m_sidebar->selectByKeys(
        {QStringLiteral("classes")}
        );
}

void SidebarController::deleteClass()
{
    int classId =
        m_sidebar->getSelectedClassId();

    if (classId <= 0)
    {
        classId =
            promptForClassToDelete();

        if (classId <= 0)
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

    const Classroom classroom =
        ds->getClassById(
            classId
            );

    if (!confirmDeleteClass(classroom))
    {
        return;
    }

    if (!m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    const QStringList selectedKeys =
        m_sidebar->selectedKeys();
    const int selectedClassId =
        m_sidebar->getSelectedClassId();

    ds->deleteClass(
        classroom.id
        );

    refreshClassSidebar();

    m_pages->classesPage()->loadClasses();

    if (selectedClassId == classroom.id)
    {
        m_sidebar->selectByKeys(
            {QStringLiteral("classes")}
            );
    }
    else
    {
        m_sidebar->selectByKeys(
            selectedKeys,
            selectedClassId
            );
    }
}

