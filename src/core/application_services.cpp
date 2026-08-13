#include "application_services.h"

#include "data/data_service.h"
#include "app/services/feature_services.h"
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

void ApplicationServices::saveDatabase()
{
    if (m_dataService)
    {
        m_dataService->save();
    }
}

Status ApplicationServices::saveDatabaseAs(
    const QString& destinationPath
    )
{
    return m_dataService
        ? m_dataService->saveAs(destinationPath)
        : Status(std::unexpected(
            QStringLiteral("Data service is unavailable.")));
}

Status ApplicationServices::exportDatabaseAs(
    const QString& destinationPath
    )
{
    return m_dataService
        ? m_dataService->exportAs(destinationPath)
        : Status(std::unexpected(
            QStringLiteral("Data service is unavailable.")));
}

DataService* ApplicationServices::dataService() const
{
    return m_dataService.get();
}

SettingsService* ApplicationServices::settingsService() const
{
    if (!m_settingsService)
    {
        DataService* legacy = dataService();
        m_settingsService = std::make_unique<SettingsService>(
            legacy ? legacy->databaseSession() : nullptr, legacy);
    }
    return m_settingsService.get();
}

TeacherService* ApplicationServices::teacherService() const
{
    if (!m_teacherService)
    {
        DataService* legacy = dataService();
        m_teacherService = std::make_unique<TeacherService>(
            legacy ? legacy->databaseSession() : nullptr, legacy);
    }
    return m_teacherService.get();
}

ClassService* ApplicationServices::classService() const
{
    if (!m_classService)
    {
        DataService* legacy = dataService();
        m_classService = std::make_unique<ClassService>(
            legacy ? legacy->databaseSession() : nullptr, legacy);
    }
    return m_classService.get();
}

ScheduleService* ApplicationServices::scheduleService() const
{
    if (!m_scheduleService)
    {
        DataService* legacy = dataService();
        m_scheduleService = std::make_unique<ScheduleService>(
            legacy ? legacy->databaseSession() : nullptr, legacy);
    }
    return m_scheduleService.get();
}

CalendarService* ApplicationServices::calendarService() const
{
    if (!m_calendarService)
    {
        DataService* legacy = dataService();
        m_calendarService = std::make_unique<CalendarService>(
            legacy ? legacy->databaseSession() : nullptr, legacy);
    }
    return m_calendarService.get();
}

RosterService* ApplicationServices::rosterService() const
{
    if (!m_rosterService)
    {
        DataService* legacy = dataService();
        m_rosterService = std::make_unique<RosterService>(
            legacy ? legacy->databaseSession() : nullptr, legacy);
    }
    return m_rosterService.get();
}

SpeakingEvaluationService* ApplicationServices::speakingEvaluationService() const
{
    if (!m_speakingEvaluationService)
    {
        DataService* legacy = dataService();
        m_speakingEvaluationService =
            std::make_unique<SpeakingEvaluationService>(
                legacy ? legacy->databaseSession() : nullptr, legacy);
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
