#include "sidebar_controller.h"

#include "domain/models/gs_team_member.h"
#include "domain/models/native_english_teacher.h"
#include "domain/models/teacher.h"
#include "features/teacher/ui/upcoming_birthdays_dialog.h"
#include "next/application/gs_team_birthday_directory_read_query.h"
#include "next/application/korean_teacher_birthday_directory_read_query.h"
#include "next/application/native_english_teacher_birthday_directory_read_query.h"
#include "next/platform/application_services_gs_team_birthday_directory_read_port.h"
#include "next/platform/application_services_korean_teacher_birthday_directory_read_port.h"
#include "next/platform/application_services_native_english_teacher_birthday_directory_read_port.h"
#include "next/platform/settings_manager_upcoming_birthday_dismissal_port.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <string>
#include <utility>

namespace
{
QString directoryReadErrorDetails(const std::string& message)
{
    return QString::fromUtf8(
        message.data(),
        static_cast<qsizetype>(message.size())
        );
}
}

std::optional<UpcomingBirthdaySchedule>
SidebarController::loadUpcomingBirthdaySchedule(
    const QDate& referenceDate
    ) const
{
    ClassMngr::Next::Platform::
        ApplicationServicesKoreanTeacherBirthdayDirectoryReadPort
            koreanTeacherReadPort(m_services);
    const ClassMngr::Next::Application::
        KoreanTeacherBirthdayDirectoryReadQuery koreanTeacherReadQuery(
            koreanTeacherReadPort);
    const ClassMngr::Next::Application::
        KoreanTeacherBirthdayDirectoryReadResult koreanTeacherDirectory =
            koreanTeacherReadQuery.execute();

    if (!koreanTeacherDirectory)
    {
        if (koreanTeacherDirectory.error().code
            == ClassMngr::Next::Domain::ErrorCode::NotFound)
        {
            return std::nullopt;
        }

        DialogServices::showWarning(
            m_sidebar,
            tr("Upcoming Birthdays"),
            tr("Birthdays could not be loaded."),
            directoryReadErrorDetails(koreanTeacherDirectory.error().message)
            );
        return std::nullopt;
    }

    QList<Teacher> koreanTeachers;
    koreanTeachers.reserve(
        static_cast<qsizetype>(koreanTeacherDirectory.value().size())
        );
    for (const auto& entry : koreanTeacherDirectory.value())
    {
        Teacher teacher;
        teacher.birthday = QString::fromStdU16String(entry.birthday);
        teacher.teacherKr = QString::fromStdU16String(entry.teacherKr);
        teacher.teacherEn = QString::fromStdU16String(entry.teacherEn);
        teacher.preferredRomanization = QString::fromStdU16String(
            entry.preferredRomanization);
        teacher.preferredName = QString::fromStdU16String(entry.preferredName);
        koreanTeachers.append(std::move(teacher));
    }

    ClassMngr::Next::Platform::
        ApplicationServicesNativeEnglishTeacherBirthdayDirectoryReadPort
            nativeEnglishBirthdayReadPort(m_services);
    const ClassMngr::Next::Application::
        NativeEnglishTeacherBirthdayDirectoryReadQuery
            nativeEnglishBirthdayReadQuery(nativeEnglishBirthdayReadPort);
    const ClassMngr::Next::Application::
        NativeEnglishTeacherBirthdayDirectoryReadResult
            nativeEnglishBirthdayDirectory =
                nativeEnglishBirthdayReadQuery.execute();

    ClassMngr::Next::Platform::
        ApplicationServicesGsTeamBirthdayDirectoryReadPort
            gsTeamBirthdayReadPort(m_services);
    const ClassMngr::Next::Application::GsTeamBirthdayDirectoryReadQuery
        gsTeamBirthdayReadQuery(gsTeamBirthdayReadPort);
    const ClassMngr::Next::Application::GsTeamBirthdayDirectoryReadResult
        gsTeamBirthdayDirectory = gsTeamBirthdayReadQuery.execute();

    if (!nativeEnglishBirthdayDirectory || !gsTeamBirthdayDirectory)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Upcoming Birthdays"),
            tr("Birthdays could not be loaded."),
            directoryReadErrorDetails(
                !nativeEnglishBirthdayDirectory
                    ? nativeEnglishBirthdayDirectory.error().message
                    : gsTeamBirthdayDirectory.error().message
                )
            );
        return std::nullopt;
    }

    QList<NativeEnglishTeacher> nativeEnglishTeachers;
    nativeEnglishTeachers.reserve(
        static_cast<qsizetype>(nativeEnglishBirthdayDirectory.value().size())
        );
    for (const auto& entry : nativeEnglishBirthdayDirectory.value())
    {
        nativeEnglishTeachers.append({
            .name = QString::fromStdU16String(entry.name),
            .position = QString::fromStdU16String(entry.position),
            .birthday = QString::fromStdU16String(entry.birthday)
        });
    }

    QList<GsTeamMember> gsTeamMembers;
    gsTeamMembers.reserve(
        static_cast<qsizetype>(gsTeamBirthdayDirectory.value().size())
        );
    for (const auto& entry : gsTeamBirthdayDirectory.value())
    {
        gsTeamMembers.append({
            .name = QString::fromStdU16String(entry.name),
            .koreanName = QString::fromStdU16String(entry.koreanName),
            .position = QString::fromStdU16String(entry.position),
            .birthday = QString::fromStdU16String(entry.birthday)
        });
    }

    return UpcomingBirthdaySchedule::build(
        koreanTeachers,
        nativeEnglishTeachers,
        gsTeamMembers,
        referenceDate
        );
}
void SidebarController::showUpcomingBirthdays()
{
    const QDate today = QDate::currentDate();
    const auto schedule = loadUpcomingBirthdaySchedule(today);
    if (!schedule)
    {
        return;
    }

    UpcomingBirthdaysDialog dialog(*schedule, m_sidebar);
    dialog.exec();

    if (dialog.dismissForToday())
    {
        const ClassMngr::Next::Application::CalendarEventDate dismissalDate(
            today.toString(Qt::ISODate).toUtf8().toStdString()
            );
        ClassMngr::Next::Platform::
            SettingsManagerUpcomingBirthdayDismissalPort()
            .write(dismissalDate);
    }
}
