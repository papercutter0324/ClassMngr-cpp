#include "sidebar_controller_p.h"

#include "app/services/feature_services.h"
#include "ui/shared/dialogs/user_prompt_service.h"

using namespace SidebarControllerPrivate;

Classroom SidebarController::getClassById(int classId) const
{
    auto* classes =
        openClassService(m_services);

    return classes
        ? classes->classroom(classId).value_or(Classroom{})
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

    const Result<int> created = classes->create(QString());
    if (!created)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Add Class"),
            tr("The class could not be created."),
            created.error()
            );
        return;
    }
    const int classId = *created;

    refreshClassSidebar();

    const Result<Classroom> classroom =
        classes->classroom(
            classId
            );

    if (!classroom)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Add Class"),
            tr("The created class could not be loaded."),
            classroom.error()
            );
        return;
    }

    if (auto* page = m_pages->ensureClassesPage())
    {
        page->openClass(
            classroom->id,
            ClassesSection::Details
            );
    }

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

    const Result<Classroom> classroom =
        classes->classroom(
            classId
            );

    if (!classroom)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Delete Class"),
            tr("The class could not be loaded."),
            classroom.error()
            );
        return;
    }

    if (!confirmDeleteClass(*classroom))
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

    const Status removed = classes->remove(classroom->id);
    if (!removed)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Delete Class"),
            tr("The class could not be deleted."),
            removed.error()
            );
        return;
    }

    refreshClassSidebar();

    if (auto* page = m_pages->classesPage())
    {
        page->loadClasses();
    }

    if (selectedClassId == classroom->id)
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
