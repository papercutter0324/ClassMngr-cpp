#include "sidebar_controller_p.h"

#include "app/services/feature_services.h"
#include "next/application/teacher_create_use_case.h"
#include "next/application/teacher_delete_use_case.h"
#include "next/application/teacher_profile_read_query.h"
#include "next/domain/domain_types.h"
#include "next/platform/application_services_teacher_create_port.h"
#include "next/platform/application_services_teacher_delete_port.h"
#include "next/platform/application_services_teacher_profile_read_port.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <limits>
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
    if (!m_pages || !m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    m_teacherCreationReturnSelection = m_sidebar
        ? m_sidebar->selectedKeys()
        : QStringList{};
    m_teacherCreationReturnTeacherId = m_sidebar
        ? m_sidebar->getSelectedTeacherId()
        : -1;

    if (!openTeacherService(m_services))
    {
        return;
    }

    ClassMngr::Next::Application::TeacherCreateRequest request;
    request.fields.teacherEn = u"New Teacher";
    request.fields.teacherKr = u"선생님";
    ClassMngr::Next::Platform::
        TeacherValidatorTeacherCreateValidationPolicy validationPolicy;
    ClassMngr::Next::Platform::ApplicationServicesTeacherCreatePort createPort(
        m_services
        );
    const auto created =
        ClassMngr::Next::Application::TeacherCreateUseCase::execute(
            request,
            validationPolicy,
            createPort
            );
    if (!created)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Add Teacher"),
            tr("The teacher could not be created."),
            QString::fromUtf8(
                created.error().message.data(),
                static_cast<qsizetype>(created.error().message.size())
                )
            );
        return;
    }

    bool teacherIdOk = false;
    const qint64 teacherId = QString::fromStdString(created.value().value())
        .toLongLong(&teacherIdOk);
    if (!teacherIdOk || teacherId <= 0)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Add Teacher"),
            tr("The teacher could not be created.")
            );
        return;
    }

    const auto typedTeacherId =
        ClassMngr::Next::Domain::TeacherId::fromString(
            created.value().value()
            );
    if (!typedTeacherId)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Add Teacher"),
            tr("The created teacher could not be loaded.")
            );
        return;
    }

    ClassMngr::Next::Platform::
        ApplicationServicesTeacherProfileReadPort readPort(m_services);
    const ClassMngr::Next::Application::TeacherProfileReadQuery query(readPort);
    const auto profile = query.execute(*typedTeacherId);
    if (!profile)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Add Teacher"),
            tr("The created teacher could not be loaded."),
            QString::fromUtf8(
                profile.error().message.data(),
                static_cast<qsizetype>(profile.error().message.size())
                )
            );
        return;
    }

    const auto& fields = profile.value().fields;
    Teacher teacher;
    teacher.id = teacherId <= std::numeric_limits<int>::max()
        ? static_cast<int>(teacherId)
        : -1;
    teacher.teacherKr = QString::fromStdU16String(fields.teacherKr);
    teacher.teacherEn = QString::fromStdU16String(fields.teacherEn);
    teacher.preferredRomanization = QString::fromStdU16String(
        fields.preferredRomanization);
    teacher.preferredName = QString::fromStdU16String(fields.preferredName);
    teacher.roomNumber = QString::fromStdU16String(fields.roomNumber);
    teacher.birthday = QString::fromStdU16String(fields.birthday);
    teacher.phoneNumber = QString::fromStdU16String(fields.phoneNumber);
    teacher.wifiName = QString::fromStdU16String(fields.wifiName);
    teacher.wifiPassword = QString::fromStdU16String(fields.wifiPassword);
    teacher.internetType = QString::fromStdU16String(fields.internetType);
    teacher.zoomId = QString::fromStdU16String(fields.zoomId);
    teacher.zoomPassword = QString::fromStdU16String(fields.zoomPassword);
    teacher.projectionType = QString::fromStdU16String(fields.projectionType);
    teacher.notes = QString::fromStdU16String(fields.notes);

    refreshTeacherSidebar();
    m_sidebar->selectTeacher(
        teacherId
        );

    auto* page = m_pages->ensureTeacherPage();

    if (!page)
    {
        return;
    }

    page->loadTeacher(
        teacher,
        teacherId
        );
    page->markCreatedTeacherPendingConfirmation();

    m_pages->showPage(
        PageType::TeacherInfo
        );
}

void SidebarController::deleteTeacher()
{
    qint64 teacherId =
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

    ClassMngr::Next::Platform::
        ApplicationServicesTeacherDeletePort deletePort(m_services);
    const ClassMngr::Next::Application::TeacherDeleteRequest deleteRequest{
        .teacherId = *typedTeacherId
    };
    const auto removed =
        ClassMngr::Next::Application::TeacherDeleteUseCase::execute(
            deleteRequest,
            deletePort
            );
    if (!removed)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Delete Teacher"),
            tr("The teacher could not be deleted."),
            QString::fromUtf8(
                removed.error().message.data(),
                static_cast<qsizetype>(removed.error().message.size())
                )
            );
        return;
    }

    refreshTeacherSidebar();
}
