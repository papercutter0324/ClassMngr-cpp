#include "schedule_import_dialog_shared.h"

#include "app/services/feature_services.h"
#include "core/application_services.h"

ScheduleService* openScheduleImportService(
    ApplicationServices* services
    )
{
    ScheduleService* scheduleService =
        services
            ? services->scheduleService()
            : nullptr;
    return scheduleService && scheduleService->isAvailable()
        ? scheduleService
        : nullptr;
}
