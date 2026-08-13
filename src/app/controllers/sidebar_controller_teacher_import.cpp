#include "sidebar_controller_p.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "app/services/feature_services.h"

using namespace SidebarControllerPrivate;

#include "features/teacher/ui/staff_directory_page.h"
#include "features/teacher/ui/teacher_import_dialog.h"


void SidebarController::importTeachers()
{
    TeacherService* teachers = openTeacherService(m_services);
    if (!teachers || !m_pages || !m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    TeacherImportDialog dialog(m_sidebar);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const TeacherImportPlan plan = dialog.importPlan();
    const QDate previousDate = teachers->latestImportDate();
    if (previousDate.isValid() && plan.sourceDate <= previousDate)
    {
        const bool versionsMatch = plan.sourceDate == previousDate;
        const QString confirmationMessage =
            (versionsMatch
                 ? tr("The selected file's version appears to match the current data. Do you wish to continue?\n"
                      "    File Version: %1\n"
                      "    Current Version: %2")
                 : tr("The selected file appears to be older version than the current data. Do you wish to continue?\n"
                      "    File Version: %1\n"
                      "    Current Version: %2"))
                .arg(plan.sourceDate.toString(Qt::ISODate),
                     previousDate.toString(Qt::ISODate));
        const PromptChoice answer = DialogServices::confirm(
            m_sidebar,
            tr("Import Teachers"),
            confirmationMessage,
            tr("Continue"),
            tr("Cancel")
            );
        if (answer != PromptChoice::Accepted)
        {
            return;
        }
    }

    const Result<TeacherImportSummary> imported =
        teachers->importTeachers(plan);
    if (!imported)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Import Teachers"),
            imported.error());
        return;
    }

    refreshTeacherSidebar();
    if (m_pages->nativeEnglishTeachersPage())
    {
        m_pages->nativeEnglishTeachersPage()->loadDirectory();
    }
    if (m_pages->gsTeamPage())
    {
        m_pages->gsTeamPage()->loadDirectory();
    }
    updateActionStates();

    const TeacherImportSummary& summary = *imported;
    DialogServices::showInformation(
        m_sidebar,
        tr("Import Teachers"),
        tr("Import complete.\n\n"
           "Korean Teachers: %1 created, %2 updated, %3 unchanged\n"
           "Native English Teachers: %4 created, %5 updated, %6 unchanged\n"
           "GS Team: %7 created, %8 updated, %9 unchanged")
            .arg(summary.koreanTeachers.created)
            .arg(summary.koreanTeachers.updated)
            .arg(summary.koreanTeachers.unchanged)
            .arg(summary.nativeEnglishTeachers.created)
            .arg(summary.nativeEnglishTeachers.updated)
            .arg(summary.nativeEnglishTeachers.unchanged)
            .arg(summary.gsTeamMembers.created)
            .arg(summary.gsTeamMembers.updated)
            .arg(summary.gsTeamMembers.unchanged));
}
