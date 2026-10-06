#include "sidebar_controller_p.h"

#include "app/services/feature_services.h"
#include "next/application/class_delete_use_case.h"
#include "next/application/classes_list_read_query.h"
#include "next/platform/application_services_class_delete_port.h"
#include "next/platform/application_services_classes_list_read_port.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <string>

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

    const auto typedClassId =
        ClassMngr::Next::Domain::ClassId::fromString(
            std::to_string(classId)
            );
    if (!typedClassId)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Delete Class"),
            tr("The class could not be loaded."),
            tr("The selected class identifier is invalid.")
            );
        return;
    }

    ClassMngr::Next::Platform::
        ApplicationServicesClassesListReadPort readPort(m_services);
    const ClassMngr::Next::Application::ClassesListReadQuery query(readPort);
    const auto loadedClasses = query.execute();
    if (!loadedClasses)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Delete Class"),
            tr("The class could not be loaded."),
            QString::fromUtf8(
                loadedClasses.error().message.data(),
                static_cast<qsizetype>(loadedClasses.error().message.size())
                )
            );
        return;
    }

    Classroom classroom;
    bool foundClass = false;
    for (const auto& entry : loadedClasses.value().classes)
    {
        if (entry.classId != *typedClassId)
        {
            continue;
        }

        classroom.id = classId;
        classroom.name = QString::fromStdU16String(entry.className);
        foundClass = true;
        break;
    }
    if (!foundClass)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Delete Class"),
            tr("The class could not be loaded."),
            tr("The selected class could not be found.")
            );
        return;
    }

    if (!confirmDeleteClass(classroom))
    {
        return;
    }

    if (!m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    ClassMngr::Next::Platform::ApplicationServicesClassDeletePort deletePort(
        m_services
        );
    const auto removed =
        ClassMngr::Next::Application::ClassDeleteUseCase::execute(
            ClassMngr::Next::Application::ClassDeleteRequest{
                .classId = *typedClassId
            },
            deletePort
            );
    if (!removed)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Delete Class"),
            tr("The class could not be deleted."),
            QString::fromUtf8(
                removed.error().message.data(),
                static_cast<qsizetype>(removed.error().message.size())
                )
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
