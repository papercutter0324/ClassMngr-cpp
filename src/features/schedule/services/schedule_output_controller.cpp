#include "schedule_output_controller.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "core/application_services.h"
#include "features/schedule/services/schedule_print_service.h"
#include "features/schedule/ui/schedule_print_dialog.h"
#include "next/platform/application_services_personal_display_name_preferences_port.h"

#include <QDialog>
#include <QObject>

#include <string>

void ScheduleOutputController::execute(
    Action action,
    QWidget* parent,
    ApplicationServices* services,
    const ScheduleViewModel& model,
    Theme currentTheme,
    bool showEnglishNames
    )
{
    const bool print = action == Action::Print;
    SchedulePrintDialog dialog(
        print
            ? SchedulePrintDialog::Action::Print
            : SchedulePrintDialog::Action::SaveAs,
        parent
        );
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    SchedulePrintService::Request request;
    request.parent = parent;
    request.model = model;
    request.showEnglishNames = showEnglishNames;
    request.style = dialog.selectedStyle();
    request.pageOrientation = dialog.selectedOrientation();
    request.currentTheme = currentTheme;
    if (services)
    {
        ClassMngr::Next::Platform::
            ApplicationServicesPersonalDisplayNamePreferencesPort
            personalDisplayNamePreferencesPort(*services);
        const std::string userName =
            personalDisplayNamePreferencesPort.read();
        request.userName = QString::fromUtf8(
            userName.data(),
            static_cast<qsizetype>(userName.size())
            );
    }

    const SchedulePrintService::Result result = print
        ? SchedulePrintService::printSchedule(request)
        : SchedulePrintService::saveSchedulePdf(
            request,
            dialog.selectedSavePath()
            );
    if (result.status == SchedulePrintService::Status::Failed)
    {
        DialogServices::showWarning(
            parent,
            print
                ? QObject::tr("Print Schedule")
                : QObject::tr("Export Schedule"),
            result.message
            );
    }
}
