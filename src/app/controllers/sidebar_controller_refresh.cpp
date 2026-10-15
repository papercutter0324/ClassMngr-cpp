#include "sidebar_controller_p.h"

#include "next/application/class_teacher_assignments_read_query.h"
#include "next/application/initial_setup_teacher_choices_read_query.h"
#include "next/platform/application_services_class_teacher_assignments_read_port.h"
#include "next/platform/application_services_initial_setup_teacher_choices_read_port.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <algorithm>
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

    struct SidebarTeacher
    {
        qint64 id;
        Teacher fields;
    };
    QList<SidebarTeacher> teachers;
    for (const auto& choice : teacherChoices.value().teachers)
    {
        const auto id = ClassMngr::Next::Application::
            parseCanonicalPositiveDecimalId64(choice.teacherId.value());
        if (!id)
        {
            continue;
        }
        Teacher fields;
        fields.teacherKr = QString::fromStdU16String(choice.teacherKr);
        fields.teacherEn = QString::fromStdU16String(choice.teacherEn);
        fields.preferredRomanization = QString::fromStdU16String(
            choice.preferredRomanization);
        fields.preferredName = QString::fromStdU16String(choice.preferredName);
        teachers.append({*id, std::move(fields)});
    }
    std::sort(teachers.begin(), teachers.end(),
        [](const SidebarTeacher& left, const SidebarTeacher& right)
        {
            if (SidebarNodeNaming::teacherDisplayLessThan(left.fields, right.fields))
            {
                return true;
            }
            if (SidebarNodeNaming::teacherDisplayLessThan(right.fields, left.fields))
            {
                return false;
            }
            return left.id < right.id;
        });

    QSet<qint64> myTeacherIds;
    for (const auto& assignment : assignments.value().assignments)
    {
        if (assignment.teacherId)
        {
            const auto id = ClassMngr::Next::Application::
                parseCanonicalPositiveDecimalId64(assignment.teacherId->value());
            if (id)
            {
                myTeacherIds.insert(*id);
            }
        }
    }
    for (const auto& teacher : teachers)
    {
        const QString displayName = SidebarNodeNaming::formatTeacherDisplayName(
            teacher.fields);
        if (myTeacherIds.contains(teacher.id))
        {
            m_sidebar->addTeacherNode(displayName, teacher.id, true);
        }
        m_sidebar->addTeacherNode(displayName, teacher.id, false);
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
    qint64 teacherId
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
