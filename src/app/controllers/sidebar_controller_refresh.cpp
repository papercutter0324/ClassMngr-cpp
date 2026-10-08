#include "sidebar_controller_p.h"

#include "next/application/class_teacher_assignments_read_query.h"
#include "next/application/initial_setup_teacher_choices_read_query.h"
#include "next/platform/application_services_class_teacher_assignments_read_port.h"
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

bool isUnavailableSessionError(
    const ClassMngr::Next::Domain::OperationError& error,
    const char* expectedMessage
    )
{
    return error.code == ClassMngr::Next::Domain::ErrorCode::NotFound
        && error.message == expectedMessage;
}
}

void SidebarController::refreshTeacherSidebar()
{
    if (!m_sidebar)
    {
        return;
    }

    m_sidebar->clearTeachers();

    ClassMngr::Next::Platform::
        ApplicationServicesInitialSetupTeacherChoicesReadPort readPort(
            m_services
            );
    const ClassMngr::Next::Application::
        InitialSetupTeacherChoicesReadQuery query(readPort);
    const auto teacherChoices = query.execute();
    ClassMngr::Next::Platform::
        ApplicationServicesClassTeacherAssignmentsReadPort
            assignmentsReadPort(m_services);
    const ClassMngr::Next::Application::
        ClassTeacherAssignmentsReadQuery assignmentsQuery(
            assignmentsReadPort);
    const auto assignments = assignmentsQuery.execute();
    if (!teacherChoices || !assignments)
    {
        const bool teacherSessionUnavailable = !teacherChoices
            && isUnavailableSessionError(
                teacherChoices.error(),
                "The active database session for initial setup teacher choices is unavailable."
                );
        const bool assignmentsSessionUnavailable = !assignments
            && isUnavailableSessionError(
                assignments.error(),
                "The active database session for class teacher assignments is unavailable."
                );
        if (teacherSessionUnavailable && assignmentsSessionUnavailable)
        {
            updateActionStates();
            return;
        }

        DialogServices::showWarning(
            m_sidebar,
            tr("Load Teachers"),
            tr("Teachers and their classes could not be loaded."),
            !teacherChoices
                ? teacherChoicesErrorMessage(
                    teacherChoices.error().message
                    )
                : teacherChoicesErrorMessage(
                    assignments.error().message
                    )
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

    for (const auto& assignment : assignments.value().assignments)
    {
        if (!assignment.teacherId)
        {
            continue;
        }

        int teacherId = 0;
        const std::string& teacherIdValue = assignment.teacherId->value();
        const auto [end, conversionError] = std::from_chars(
            teacherIdValue.data(),
            teacherIdValue.data() + teacherIdValue.size(),
            teacherId
            );

        if (
            conversionError != std::errc{}
            || end != teacherIdValue.data() + teacherIdValue.size()
            || teacherId <= 0
            || std::to_string(teacherId) != teacherIdValue
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
        !assignments.value().assignments.empty(),
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
    const QStringList selectedKeys = m_sidebar
        ? m_sidebar->selectedKeys()
        : QStringList{};
    const bool selectedTeacherIsSaved = m_sidebar
        && m_sidebar->getSelectedTeacherId() == teacherId;

    refreshTeacherSidebar();

    if (!m_sidebar)
    {
        return;
    }

    if (selectedTeacherIsSaved && !selectedKeys.isEmpty())
    {
        m_sidebar->selectByKeys(selectedKeys, teacherId);
    }
    else
    {
        m_sidebar->selectTeacher(teacherId);
    }
}
