#include "application_services.h"

#include "data/data_service.h"
#include "data/database/database_file_operations.h"
#include "data/database/database_session.h"
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
    m_databaseSession = std::make_unique<DatabaseSession>();
    m_dataService = std::make_unique<DataService>(*m_databaseSession);

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
    if (!m_databaseSession)
    {
        return std::unexpected(
            QStringLiteral("Data service is unavailable.")
            );
    }

    return m_databaseSession->open(databasePath);
}

void ApplicationServices::closeDatabase()
{
    if (m_databaseSession)
    {
        m_databaseSession->close();
    }
}

bool ApplicationServices::hasOpenDatabase() const
{
    return m_databaseSession
        && m_databaseSession->isOpen();
}

QString ApplicationServices::currentDatabasePath() const
{
    return m_databaseSession
        ? m_databaseSession->databasePath()
        : QString();
}

void ApplicationServices::saveDatabase()
{
    if (hasOpenDatabase())
    {
        m_databaseSession->database().commit();
    }
}

Status ApplicationServices::saveDatabaseAs(
    const QString& destinationPath
    )
{
    if (!m_databaseSession)
    {
        return std::unexpected(
            QStringLiteral("Data service is unavailable.")
            );
    }
    if (!m_databaseSession->isOpen())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return DatabaseFileOperations::copyDatabaseFile(
        m_databaseSession->databasePath(),
        destinationPath
        );
}

Status ApplicationServices::exportDatabaseAs(
    const QString& destinationPath
    )
{
    if (!m_databaseSession)
    {
        return std::unexpected(
            QStringLiteral("Data service is unavailable.")
            );
    }
    if (!m_databaseSession->isOpen())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    return DatabaseFileOperations::copyDatabaseFile(
        m_databaseSession->databasePath(),
        destinationPath
        );
}

DataService* ApplicationServices::dataService() const
{
    return m_dataService.get();
}

DatabaseSession* ApplicationServices::databaseSession() const
{
    return m_databaseSession.get();
}

SettingsService* ApplicationServices::settingsService() const
{
    if (!m_settingsService)
    {
        m_settingsService = std::make_unique<SettingsService>(
            m_databaseSession.get(), nullptr);
    }
    return m_settingsService.get();
}

TeacherService* ApplicationServices::teacherService() const
{
    if (!m_teacherService)
    {
        m_teacherService = std::make_unique<TeacherService>(
            m_databaseSession.get(), nullptr);
    }
    return m_teacherService.get();
}

ClassService* ApplicationServices::classService() const
{
    if (!m_classService)
    {
        m_classService = std::make_unique<ClassService>(
            m_databaseSession.get(), nullptr);
    }
    return m_classService.get();
}

ScheduleService* ApplicationServices::scheduleService() const
{
    if (!m_scheduleService)
    {
        m_scheduleService = std::make_unique<ScheduleService>(
            m_databaseSession.get(), nullptr);
    }
    return m_scheduleService.get();
}

CalendarService* ApplicationServices::calendarService() const
{
    if (!m_calendarService)
    {
        m_calendarService = std::make_unique<CalendarService>(
            m_databaseSession.get(), nullptr);
    }
    return m_calendarService.get();
}

RosterService* ApplicationServices::rosterService() const
{
    if (!m_rosterService)
    {
        m_rosterService = std::make_unique<RosterService>(
            m_databaseSession.get(), nullptr);
    }
    return m_rosterService.get();
}

SpeakingEvaluationService* ApplicationServices::speakingEvaluationService() const
{
    if (!m_speakingEvaluationService)
    {
        m_speakingEvaluationService =
            std::make_unique<SpeakingEvaluationService>(
                m_databaseSession.get(), nullptr);
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
