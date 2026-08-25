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

    const Result<QList<NativeEnglishTeacher>> nativeEnglishTeachers =
        teachers->nativeEnglishTeachers();
    const Result<QList<GsTeamMember>> gsTeamMembers =
        teachers->gsTeamMembers();
    if (!nativeEnglishTeachers || !gsTeamMembers)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Upcoming Birthdays"),
            tr("Birthdays could not be loaded."),
            !nativeEnglishTeachers
                ? nativeEnglishTeachers.error()
                : gsTeamMembers.error()
            );
        return std::nullopt;
    }

    return UpcomingBirthdaySchedule::build(
        *loadedTeachers,
        *nativeEnglishTeachers,
        *gsTeamMembers,
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
        SettingsManager::instance().set(
            SettingsManager::Keys::UPCOMING_BIRTHDAYS_DISMISSED_DATE,
            today
            );
    }
}

void SidebarController::showUpcomingBirthdaysIfRelevantOnStartup()
{
    const QDate today = QDate::currentDate();
    const QDate dismissedDate =
        SettingsManager::instance().get(
            SettingsManager::Keys::UPCOMING_BIRTHDAYS_DISMISSED_DATE
            ).toDate();
    if (dismissedDate == today)
    {
        return;
    }

    const auto schedule = loadUpcomingBirthdaySchedule(today);
    if (!schedule || schedule->isEmpty())
    {
        return;
    }

    UpcomingBirthdaysDialog dialog(*schedule, m_sidebar);
    dialog.exec();

    if (dialog.dismissForToday())
    {
        SettingsManager::instance().set(
            SettingsManager::Keys::UPCOMING_BIRTHDAYS_DISMISSED_DATE,
            today
            );
    }
}
