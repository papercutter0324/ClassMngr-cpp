#pragma once

#include "next/application/gs_team_birthday_directory_read_port.h"
#include "next/application/korean_teacher_birthday_directory_read_port.h"
#include "next/application/native_english_teacher_birthday_directory_read_port.h"
#include "next/application/notification_port.h"
#include "next/application/upcoming_birthday_dismissal_port.h"
#include "next/application/upcoming_birthday_schedule_use_case.h"

#include <QDate>

#include <functional>

namespace ClassMngr::Next::Ui
{

// Composes one manual birthday invocation. Repository reads and dialog
// presentation are supplied at the boundary so failures, local dates, and
// dismissal persistence remain independently testable.
class UpcomingBirthdaysFlow final
{
public:
    using DialogPresenter = std::function<bool(
        const Application::UpcomingBirthdaySchedule&)>;

    UpcomingBirthdaysFlow(
        const Application::KoreanTeacherBirthdayDirectoryReadPort& koreanTeachers,
        const Application::NativeEnglishTeacherBirthdayDirectoryReadPort& nativeEnglishTeachers,
        const Application::GsTeamBirthdayDirectoryReadPort& gsTeam,
        const Application::UpcomingBirthdayDismissalPort& dismissal,
        Application::NotificationPort& notifications,
        DialogPresenter presentDialog
        );

    void run(const QDate& localDate) const;

private:
    const Application::KoreanTeacherBirthdayDirectoryReadPort* m_koreanTeachers;
    const Application::NativeEnglishTeacherBirthdayDirectoryReadPort* m_nativeEnglishTeachers;
    const Application::GsTeamBirthdayDirectoryReadPort* m_gsTeam;
    const Application::UpcomingBirthdayDismissalPort* m_dismissal;
    Application::NotificationPort* m_notifications;
    DialogPresenter m_presentDialog;
};

} // namespace ClassMngr::Next::Ui
