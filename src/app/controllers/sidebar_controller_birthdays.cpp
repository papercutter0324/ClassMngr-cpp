#include "sidebar_controller_p.h"

#include "app/services/feature_services.h"
#include "features/teacher/ui/upcoming_birthdays_dialog.h"
#include "ui/shared/dialogs/user_prompt_service.h"

using namespace SidebarControllerPrivate;

std::optional<UpcomingBirthdaySchedule>
SidebarController::loadUpcomingBirthdaySchedule(
    const QDate& referenceDate
    ) const
{
    auto* teachers = openTeacherService(m_services);
    if (!teachers)
    {
        return std::nullopt;
    }

    const Result<QList<Teacher>> loadedTeachers = teachers->teachers();
    if (!loadedTeachers)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Upcoming Birthdays"),
            tr("Birthdays could not be loaded."),
            loadedTeachers.error()
            );
        return std::nullopt;
    }

    return UpcomingBirthdaySchedule::build(
        *loadedTeachers,
        teachers->nativeEnglishTeachers(),
        teachers->gsTeamMembers(),
        referenceDate
        );
}

void SidebarController::showUpcomingBirthdays()
{
    const auto schedule = loadUpcomingBirthdaySchedule(QDate::currentDate());
    if (!schedule)
    {
        return;
    }

    UpcomingBirthdaysDialog dialog(*schedule, m_sidebar);
    dialog.exec();
}

void SidebarController::showUpcomingBirthdaysIfRelevantOnStartup()
{
    const auto schedule = loadUpcomingBirthdaySchedule(QDate::currentDate());
    if (!schedule || schedule->isEmpty())
    {
        return;
    }

    UpcomingBirthdaysDialog dialog(*schedule, m_sidebar);
    dialog.exec();
}
