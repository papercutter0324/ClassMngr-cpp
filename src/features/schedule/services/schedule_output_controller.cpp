#include "schedule_output_controller.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "core/application_services.h"
#include "core/theme_service.h"
#include "data/data_service.h"
#include "features/schedule/services/schedule_print_service.h"
#include "features/schedule/ui/schedule_print_dialog.h"

#include <QDialog>
#include <QObject>

void ScheduleOutputController::execute(
    Action action,
    QWidget* parent,
    ApplicationServices* services,
    const ScheduleViewModel& model,
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
    if (services && services->themeService())
    {
        request.currentTheme = services->themeService()->currentTheme();
    }
    DataService* dataService = services ? services->dataService() : nullptr;
    if (dataService && dataService->isOpen())
    {
        request.userName = dataService->loadSetting(
            QStringLiteral("myInfo/name"),
            QString()
            ).toString();
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
