#include "application_services.h"

#include "data/data_service.h"
#include "data/database/database_file_operations.h"
#include "data/database/workspace_store.h"
#include "app/services/feature_services.h"
#include "core/theme_service.h"

#include <QDebug>

ApplicationServices::ApplicationServices()
    : ApplicationServices(nullptr)
{
}

ApplicationServices::ApplicationServices(
    std::unique_ptr<ThemeService> themeService
    )
{
    m_workspaceStore = std::make_unique<WorkspaceStore>();
    m_dataService = std::make_unique<DataService>(*m_workspaceStore);

    m_themeService =
        themeService
            ? std::move(themeService)
            : std::make_unique<ThemeService>();

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
    if (!m_workspaceStore)
    {
        return std::unexpected(
            QStringLiteral("Data service is unavailable.")
            );
    }

    return m_workspaceStore->open(databasePath);
}

Status ApplicationServices::openExistingDatabase(
    const QString& databasePath
    )
{
    if (!m_workspaceStore)
    {
        return std::unexpected(
            QStringLiteral("Data service is unavailable.")
            );
    }

    return m_workspaceStore->openExisting(databasePath);
}

void ApplicationServices::closeDatabase()
{
    if (m_workspaceStore)
    {
        m_workspaceStore->close();
    }
}

bool ApplicationServices::hasOpenDatabase() const
{
    return m_workspaceStore
        && m_workspaceStore->isOpen();
}

QString ApplicationServices::currentDatabasePath() const
{
    return m_workspaceStore
        ? m_workspaceStore->databasePath()
        : QString();
}

void ApplicationServices::saveDatabase()
{
    if (hasOpenDatabase())
    {
        m_workspaceStore->database().commit();
    }
}

Status ApplicationServices::saveDatabaseAs(
    const QString& destinationPath
    )
{
    if (!m_workspaceStore)
    {
        return std::unexpected(
            QStringLiteral("Data service is unavailable.")
            );
    }
    if (!m_workspaceStore->isOpen())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return DatabaseFileOperations::copyDatabaseFile(
        m_workspaceStore->databasePath(),
        destinationPath
        );
}

Status ApplicationServices::exportDatabaseAs(
    const QString& destinationPath
    )
{
    if (!m_workspaceStore)
    {
        return std::unexpected(
            QStringLiteral("Data service is unavailable.")
            );
    }
    if (!m_workspaceStore->isOpen())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return DatabaseFileOperations::copyDatabaseFile(
        m_workspaceStore->databasePath(),
        destinationPath
        );
}

DataService* ApplicationServices::dataService() const
{
    return m_dataService.get();
}

WorkspaceStore* ApplicationServices::workspaceStore() const
{
    return m_workspaceStore.get();
}

DatabaseSession* ApplicationServices::databaseSession() const
{
    return m_workspaceStore.get();
}

SettingsService* ApplicationServices::settingsService() const
{
    if (!m_settingsService)
    {
        m_settingsService = std::make_unique<SettingsService>(
            m_workspaceStore.get(), nullptr);
    }
    return m_settingsService.get();
}

TeacherService* ApplicationServices::teacherService() const
{
    if (!m_teacherService)
    {
        m_teacherService = std::make_unique<TeacherService>(
            m_workspaceStore.get(), nullptr);
    }
    return m_teacherService.get();
}

ClassService* ApplicationServices::classService() const
{
    if (!m_classService)
    {
        m_classService = std::make_unique<ClassService>(
            m_workspaceStore.get(), nullptr);
    }
    return m_classService.get();
}

ScheduleService* ApplicationServices::scheduleService() const
{
    if (!m_scheduleService)
    {
        m_scheduleService = std::make_unique<ScheduleService>(
            m_workspaceStore.get(), nullptr);
    }
    return m_scheduleService.get();
}

CalendarService* ApplicationServices::calendarService() const
{
    if (!m_calendarService)
    {
        m_calendarService = std::make_unique<CalendarService>(
            m_workspaceStore.get(), nullptr);
    }
    return m_calendarService.get();
}

RosterService* ApplicationServices::rosterService() const
{
    if (!m_rosterService)
    {
        m_rosterService = std::make_unique<RosterService>(
            m_workspaceStore.get(), nullptr);
    }
    return m_rosterService.get();
}

SpeakingEvaluationService* ApplicationServices::speakingEvaluationService() const
{
    if (!m_speakingEvaluationService)
    {
        m_speakingEvaluationService =
            std::make_unique<SpeakingEvaluationService>(
                m_workspaceStore.get(), nullptr);
    }
    return m_speakingEvaluationService.get();
}

ThemeService* ApplicationServices::themeService() const
{
    return m_themeService.get();
}

const DocumentCatalog* ApplicationServices::documentCatalog() const
{
    return m_documentCatalog.get();
}
