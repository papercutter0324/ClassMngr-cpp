#include "application_services.h"

#include "classmngr/engine/file_system.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "app/services/feature_services.h"
#include "core/theme_service.h"

#include <QByteArray>
#include <QDebug>

#include <cstddef>
#include <string>
#include <string_view>

namespace
{

std::string utf8Path(
    const QString& path
    )
{
    const QByteArray encoded = path.toUtf8();
    return std::string(
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
        );
}

QString displayPath(
    const std::string& normalizedPath
    )
{
    return QString::fromUtf8(
        normalizedPath.data(),
        static_cast<qsizetype>(normalizedPath.size())
        );
}

Status copyDatabaseFile(
    const QString& sourcePath,
    const QString& destinationPath
    )
{
    classmngr::engine::StandardFileSystem fileSystem;
    const std::string sourceUtf8 = utf8Path(sourcePath);
    const std::string destinationUtf8 = utf8Path(destinationPath);

    const classmngr::engine::Result<std::string> normalizedDestination =
        fileSystem.normalizePath(destinationUtf8);
    const QString targetPath = normalizedDestination
        ? displayPath(*normalizedDestination)
        : destinationPath;

    const classmngr::engine::Result<std::string> normalizedSource =
        fileSystem.normalizePath(sourceUtf8);
    if (!normalizedSource || !normalizedDestination)
    {
        return std::unexpected(
            QStringLiteral("Unable to copy Teacher Profile to:\n%1")
                .arg(targetPath)
            );
    }

    if (*normalizedSource == *normalizedDestination)
    {
        return {};
    }

    const classmngr::engine::Status copied = fileSystem.copyFile(
        *normalizedSource,
        *normalizedDestination,
        true
        );
    if (copied)
    {
        return {};
    }

    const std::string_view errorToken = copied.error().message;
    if (errorToken
        == classmngr::engine::FileSystemErrorToken::DirectoryCreationFailed)
    {
        return std::unexpected(
            QStringLiteral("Unable to create destination directory:\n%1")
                .arg(targetPath)
            );
    }

    if (errorToken
        == classmngr::engine::FileSystemErrorToken::AtomicReplacementFailed)
    {
        return std::unexpected(
            QStringLiteral(
                "Unable to replace existing Teacher Profile file:\n%1"
                )
                .arg(targetPath)
            );
    }

    return std::unexpected(
        QStringLiteral("Unable to copy Teacher Profile to:\n%1")
            .arg(targetPath)
        );
}

} // namespace

ApplicationServices::ApplicationServices()
    : ApplicationServices(nullptr)
{
}

ApplicationServices::ApplicationServices(
    std::unique_ptr<ThemeService> themeService
    )
{
    m_session =
        std::make_unique<DatabaseSession>();

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
    if (!m_session)
    {
        return std::unexpected(
            QStringLiteral("Database session is unavailable.")
            );
    }

    return m_legacyDataService
        ? m_legacyDataService->openDatabase(databasePath)
        : m_session->open(databasePath);
}

void ApplicationServices::closeDatabase()
{
    if (m_session)
    {
        if (m_legacyDataService)
        {
            m_legacyDataService->closeDatabase();
        }
        else
        {
            m_session->close();
        }
    }
}

bool ApplicationServices::hasOpenDatabase() const
{
    return m_session
        && m_session->isOpen();
}

QString ApplicationServices::currentDatabasePath() const
{
    return m_session
        ? m_session->databasePath()
        : QString();
}

void ApplicationServices::saveDatabase()
{
    if (hasOpenDatabase())
    {
        m_session->database().commit();
    }
}

Status ApplicationServices::saveDatabaseAs(
    const QString& destinationPath
    )
{
    if (!hasOpenDatabase())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    if (destinationPath.trimmed().isEmpty())
    {
        return std::unexpected(
            QStringLiteral("No destination path was provided.")
            );
    }

    return copyDatabaseFile(
        m_session->databasePath(),
        destinationPath
        );
}

Status ApplicationServices::exportDatabaseAs(
    const QString& destinationPath
    )
{
    if (!hasOpenDatabase())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile is open.")
            );
    }

    if (destinationPath.trimmed().isEmpty())
    {
        return std::unexpected(
            QStringLiteral("No destination path was provided.")
            );
    }

    return copyDatabaseFile(
        m_session->databasePath(),
        destinationPath
        );
}

DataService* ApplicationServices::dataService() const
{
    if (!m_legacyDataService)
    {
        m_legacyDataService = std::make_unique<DataService>(*m_session);
    }

    return m_legacyDataService.get();
}

SettingsService* ApplicationServices::settingsService() const
{
    if (!m_settingsService)
    {
        m_settingsService = std::make_unique<SettingsService>(
            m_session.get(), nullptr);
    }
    return m_settingsService.get();
}

TeacherService* ApplicationServices::teacherService() const
{
    if (!m_teacherService)
    {
        m_teacherService = std::make_unique<TeacherService>(
            m_session.get(), nullptr);
    }
    return m_teacherService.get();
}

ClassService* ApplicationServices::classService() const
{
    if (!m_classService)
    {
        m_classService = std::make_unique<ClassService>(
            m_session.get(), nullptr);
    }
    return m_classService.get();
}

ScheduleService* ApplicationServices::scheduleService() const
{
    if (!m_scheduleService)
    {
        m_scheduleService = std::make_unique<ScheduleService>(
            m_session.get(), nullptr);
    }
    return m_scheduleService.get();
}

CalendarService* ApplicationServices::calendarService() const
{
    if (!m_calendarService)
    {
        m_calendarService = std::make_unique<CalendarService>(
            m_session.get(), nullptr);
    }
    return m_calendarService.get();
}

RosterService* ApplicationServices::rosterService() const
{
    if (!m_rosterService)
    {
        m_rosterService = std::make_unique<RosterService>(
            m_session.get(), nullptr);
    }
    return m_rosterService.get();
}

SpeakingEvaluationService* ApplicationServices::speakingEvaluationService() const
{
    if (!m_speakingEvaluationService)
    {
        m_speakingEvaluationService =
            std::make_unique<SpeakingEvaluationService>(
            m_session.get(), nullptr);
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
