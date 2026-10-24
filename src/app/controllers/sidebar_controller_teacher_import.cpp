#include "sidebar_controller_p.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "app/services/feature_services.h"

using namespace SidebarControllerPrivate;

#include "features/teacher/ui/staff_directory_page.h"
#include "features/teacher/ui/teacher_import_apply_error_display.h"
#include "features/teacher/ui/teacher_import_dialog.h"
#include "next/application/teacher_import_latest_source_date_read_query.h"
#include "next/domain/gregorian_date.h"
#include "next/platform/application_services_teacher_profile_read_port.h"
#include "next/platform/application_services_teacher_import_latest_source_date_read_port.h"
#include "next/platform/application_services_teacher_import_apply_port.h"

#include <optional>
#include <QStringList>


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
    ClassMngr::Next::Platform::
        ApplicationServicesTeacherImportLatestSourceDateReadPort readPort(
            m_services);
    const ClassMngr::Next::Application::
        TeacherImportLatestSourceDateReadQuery query(readPort);
    const ClassMngr::Next::Application::
        TeacherImportLatestSourceDateReadResult previousDateResult =
            query.execute();
    if (!previousDateResult)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Import Teachers"),
            QString::fromUtf8(
                previousDateResult.error().message.data(),
                static_cast<qsizetype>(
                    previousDateResult.error().message.size()))
            );
        return;
    }

    const std::optional<ClassMngr::Next::Domain::GregorianDate>& previousDateValue =
        previousDateResult.value();
    const QDate previousDate = previousDateValue
        ? QDate(
              previousDateValue->year(),
              previousDateValue->month(),
              previousDateValue->day())
        : QDate();
    if (previousDate.isValid()
        && plan.sourceDate.isValid()
        && plan.sourceDate <= previousDate)
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

    const ClassMngr::Next::Platform::
        ApplicationServicesTeacherImportApplyPort applyPort(m_services);
    const ClassMngr::Next::Platform::TeacherImportApplyPortResult
        imported = applyPort.apply(plan);
    if (!imported)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Import Teachers"),
            ClassMngr::Features::TeacherUi::teacherImportApplyErrorDisplayText(
                imported.error()));
        return;
    }

    const ClassMngr::Next::Application::TeacherImportUseCaseResult& summary =
        *imported;
    bool teacherInfoReloadFailed = false;
    if (summary.koreanTeachers.updated.rawValue() > 0
        && m_pages->isCurrentPage(PageType::TeacherInfo))
    {
        TeacherInfoPage* const teacherPage = m_pages->teacherPage();
        if (teacherPage && teacherPage->teacherId() > 0)
        {
            const ClassMngr::Next::Platform::
                ApplicationServicesTeacherProfileReadPort readPort(m_services);
            teacherInfoReloadFailed =
                !teacherPage->reloadTeacherProfile(readPort);
        }
    }

    refreshTeacherSidebar();
    QStringList directoryRefreshFailures;
    if (StaffDirectoryPage* const page =
            m_pages->nativeEnglishTeachersPage())
    {
        const bool changed =
            summary.nativeEnglishTeachers.created.rawValue() > 0
            || summary.nativeEnglishTeachers.updated.rawValue() > 0;
        QString readError;
        if (!page->refreshAfterTeacherImport(changed, &readError) && changed)
        {
            directoryRefreshFailures.append(
                tr("Native English Teachers: %1").arg(readError));
        }
    }
    if (StaffDirectoryPage* const page = m_pages->gsTeamPage())
    {
        const bool changed =
            summary.gsTeamMembers.created.rawValue() > 0
            || summary.gsTeamMembers.updated.rawValue() > 0;
        QString readError;
        if (!page->refreshAfterTeacherImport(changed, &readError) && changed)
        {
            directoryRefreshFailures.append(
                tr("GS Team: %1").arg(readError));
        }
    }
    updateActionStates();

    DialogServices::showInformation(
        m_sidebar,
        tr("Import Teachers"),
        tr("Import complete.\n\n"
           "Korean Teachers: %1 created, %2 updated, %3 unchanged\n"
           "Native English Teachers: %4 created, %5 updated, %6 unchanged\n"
           "GS Team: %7 created, %8 updated, %9 unchanged")
            .arg(summary.koreanTeachers.created.rawValue())
            .arg(summary.koreanTeachers.updated.rawValue())
            .arg(summary.koreanTeachers.unchanged.rawValue())
            .arg(summary.nativeEnglishTeachers.created.rawValue())
            .arg(summary.nativeEnglishTeachers.updated.rawValue())
            .arg(summary.nativeEnglishTeachers.unchanged.rawValue())
            .arg(summary.gsTeamMembers.created.rawValue())
            .arg(summary.gsTeamMembers.updated.rawValue())
            .arg(summary.gsTeamMembers.unchanged.rawValue()));

    if (!directoryRefreshFailures.isEmpty())
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Import Teachers"),
            tr("The teacher import committed its changes, but a staff directory refresh failed. "
               "Reload the affected directory before editing or saving.\n\n%1")
                .arg(directoryRefreshFailures.join(QStringLiteral("\n")))
            );
    }

    if (teacherInfoReloadFailed)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Import Teachers"),
            tr("The teacher import succeeded, but the active Teacher Info page could not be refreshed. Navigate away and reopen this teacher profile to load the imported data before editing or saving."));
    }
}
