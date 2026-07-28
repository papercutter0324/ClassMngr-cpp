#include "schedule_import_dialog_shared.h"

#include "core/application_services.h"
#include "data/data_service.h"

DataService* openScheduleImportDataService(
    ApplicationServices* services
    )
{
    DataService* dataService =
        services
            ? services->dataService()
            : nullptr;
    return dataService && dataService->isOpen()
        ? dataService
        : nullptr;
}
