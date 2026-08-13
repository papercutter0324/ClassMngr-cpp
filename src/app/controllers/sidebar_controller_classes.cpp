#include "sidebar_controller_p.h"

#include "app/services/feature_services.h"

using namespace SidebarControllerPrivate;

Classroom SidebarController::getClassById(int classId) const
{
    auto* classes =
        openClassService(m_services);

    return classes
        ? classes->classroom(classId)
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
    auto* classes =
        openClassService(m_services);

    if (!classes)
    {
        return;
    }

    if (!m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    int classId =
        classes->create(QString());

    refreshClassSidebar();

    Classroom classroom =
        classes->classroom(
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

    auto* classes =
        openClassService(m_services);

    if (!classes)
    {
        return;
    }

    const Classroom classroom =
        classes->classroom(
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

    classes->remove(
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

