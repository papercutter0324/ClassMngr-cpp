#include "application_services.h"

#include "data/data_service.h"
#include "core/theme_service.h"

#include <QDebug>

ApplicationServices::ApplicationServices()
{
    m_dataService =
        std::make_unique<DataService>();

    m_themeService =
        std::make_unique<ThemeService>();

    m_documentCatalog =
        std::make_unique<DocumentCatalog>();

    [[maybe_unused]] const Status catalogStatus =
        m_documentCatalog->initialize();

    for (const QString& warning : m_documentCatalog->warnings())
    {
        qWarning().noquote()
            << warning;
    }
}

ApplicationServices::~ApplicationServices() = default;

Status ApplicationServices::openDatabase(
    const QString& databasePath
    )
{
    if (!m_dataService)
    {
        return std::unexpected(
            QStringLiteral("Data service is unavailable.")
            );
    }

    return m_dataService->openDatabase(databasePath);
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

const DocumentCatalog* ApplicationServices::documentCatalog() const
{
    return m_documentCatalog.get();
}
