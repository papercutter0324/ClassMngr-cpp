#include "sidebar_controller_p.h"

#include "app/services/feature_services.h"
#include "ui/shared/dialogs/user_prompt_service.h"

using namespace SidebarControllerPrivate;

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
    const int classId =
        promptForClassToDelete();

    if (classId <= 0)
    {
        return;
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

    if (auto* page = m_pages->classesPage())
    {
        page->loadClasses();
    }

    m_sidebar->selectByKeys(
        {QStringLiteral("classes")}
        );
}
