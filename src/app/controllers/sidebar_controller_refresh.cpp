#include "sidebar_controller_p.h"

#include "app/services/feature_services.h"
#include "next/application/initial_setup_teacher_choices_read_query.h"
#include "next/platform/application_services_initial_setup_teacher_choices_read_port.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <charconv>
#include <string>
#include <system_error>
#include <utility>

using namespace SidebarControllerPrivate;

namespace
{
QString teacherChoicesErrorMessage(const std::string& message)
{
    return QString::fromUtf8(
        message.data(),
        static_cast<qsizetype>(message.size())
        );
}
}

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

    ClassMngr::Next::Platform::
        ApplicationServicesInitialSetupTeacherChoicesReadPort readPort(
            m_services
            );
    const ClassMngr::Next::Application::
        InitialSetupTeacherChoicesReadQuery query(readPort);
    const auto teacherChoices = query.execute();
    const Result<QList<ClassTeacherAssignment>> assignments =
        classes->classTeacherAssignments();
    if (!teacherChoices || !assignments)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Load Teachers"),
            tr("Teachers and their classes could not be loaded."),
            !teacherChoices
                ? teacherChoicesErrorMessage(
                    teacherChoices.error().message
                    )
                : assignments.error()
            );
        updateActionStates();
        return;
    }

    QList<Teacher> teachers;
    teachers.reserve(
        static_cast<qsizetype>(teacherChoices.value().teachers.size())
        );
    for (const auto& choice : teacherChoices.value().teachers)
    {
        int teacherId = 0;
        const std::string& teacherIdValue = choice.teacherId.value();
        const auto [end, conversionError] = std::from_chars(
            teacherIdValue.data(),
            teacherIdValue.data() + teacherIdValue.size(),
            teacherId
            );
        if (conversionError != std::errc{}
            || end != teacherIdValue.data() + teacherIdValue.size()
            || teacherId <= 0
            || std::to_string(teacherId) != teacherIdValue)
        {
            continue;
        }

        Teacher teacher;
        teacher.id = teacherId;
        teacher.teacherKr = QString::fromStdU16String(choice.teacherKr);
        teacher.teacherEn = QString::fromStdU16String(choice.teacherEn);
        teacher.preferredRomanization = QString::fromStdU16String(
            choice.preferredRomanization
            );
        teacher.preferredName = QString::fromStdU16String(
            choice.preferredName
            );
        teachers.append(std::move(teacher));
    }

    QHash<int, Teacher> teachersById;

    for (const Teacher& teacher : teachers)
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

    for (const Teacher& teacher : sortedTeachers(teachers))
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
        !teachers.isEmpty()
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
