#include "sidebar_controller_p.h"

#include "app/services/feature_services.h"
#include "next/application/teacher_profile_read_query.h"
#include "next/domain/domain_types.h"
#include "next/platform/application_services_teacher_profile_read_port.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <string>

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

    auto* page = m_pages->ensureTeacherPage();

    if (!page)
    {
        return;
    }

    page->loadTeacher(
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

    const auto typedTeacherId =
        ClassMngr::Next::Domain::TeacherId::fromString(
            std::to_string(teacherId)
            );
    if (!typedTeacherId)
    {
        return;
    }

    ClassMngr::Next::Platform::
        ApplicationServicesTeacherProfileReadPort readPort(m_services);
    const ClassMngr::Next::Application::TeacherProfileReadQuery query(
        readPort
        );
    const auto profile = query.execute(*typedTeacherId);

    if (!profile)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Delete Teacher"),
            tr("The teacher could not be loaded."),
            QString::fromUtf8(
                profile.error().message.data(),
                static_cast<qsizetype>(profile.error().message.size())
                )
            );
        return;
    }

    const auto& fields = profile.value().fields;
    Teacher teacher;
    teacher.id = teacherId;
    teacher.teacherKr = QString::fromStdU16String(fields.teacherKr);
    teacher.teacherEn = QString::fromStdU16String(fields.teacherEn);
    teacher.preferredRomanization = QString::fromStdU16String(
        fields.preferredRomanization
        );
    teacher.preferredName = QString::fromStdU16String(fields.preferredName);

    if (!confirmDeleteTeacher(teacher))
    {
        return;
    }

    const Status removed = teachers->remove(teacher.id);
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
}
