#include "application_services.h"

#include "data/data_service.h"
#include "core/theme_service.h"

ApplicationServices::ApplicationServices()
{
    m_dataService =
        std::make_unique<DataService>();

    m_themeService =
        std::make_unique<ThemeService>();
}

ApplicationServices::~ApplicationServices() = default;

bool ApplicationServices::openDatabase(
    const QString& databasePath
    )
{
    return m_dataService
        && m_dataService->openDatabase(databasePath);
}

void ApplicationServices::closeDatabase()
{
    if (m_dataService)
    {
        m_dataService->closeDatabase();
    }
}

bool ApplicationServices::hasOpenDatabase() const
{
    return m_dataService
        && m_dataService->isOpen();
}

QString ApplicationServices::currentDatabasePath() const
{
    return m_dataService
        ? m_dataService->currentDatabasePath()
        : QString();
}

DataService* ApplicationServices::dataService() const
{
    return m_dataService.get();
}

ThemeService* ApplicationServices::themeService() const
{
    return m_themeService.get();
}
