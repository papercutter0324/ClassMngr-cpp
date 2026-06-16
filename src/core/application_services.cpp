#include "application_services.h"

#include "data/data_service.h"
#include "core/theme_service.h"

ApplicationServices::ApplicationServices(
    const QString& databasePath
    )
{
    m_dataService =
        std::make_unique<DataService>(
            databasePath
            );

    m_dataService->open();

    m_themeService =
        std::make_unique<ThemeService>();
}

ApplicationServices::~ApplicationServices() = default;

DataService* ApplicationServices::dataService() const
{
    return m_dataService.get();
}

ThemeService* ApplicationServices::themeService() const
{
    return m_themeService.get();
}