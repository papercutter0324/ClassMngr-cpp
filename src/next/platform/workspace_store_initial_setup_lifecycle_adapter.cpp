#include "next/platform/workspace_store_initial_setup_lifecycle_adapter.h"
#include "next/platform/workspace_profile_path_policy.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

#include <cstddef>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{
namespace
{

[[nodiscard]] QString toQString(const std::string& value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

[[nodiscard]] QString toQString(
    const Application::WorkspaceLocation& location
    )
{
    return toQString(location.value());
}

[[nodiscard]] std::string toUtf8(const QString& value)
{
    const QByteArray encoded = value.toUtf8();
    return std::string(
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
        );
}

[[nodiscard]] Domain::OperationError technicalError(
    const QString& message,
    const bool recoverable = true
    )
{
    return Domain::OperationError{
        .code = Domain::ErrorCode::Technical,
        .message = toUtf8(message),
        .recoverable = recoverable
    };
}

[[nodiscard]] Domain::OperationError invalidInputError(const QString& message)
{
    return Domain::OperationError{
        .code = Domain::ErrorCode::InvalidInput,
        .message = toUtf8(message),
        .recoverable = false
    };
}

[[nodiscard]] bool isAbsolutePath(const QString& path)
{
    return QFileInfo(path).isAbsolute();
}

[[nodiscard]] QJsonObject recordObject(
    const Application::InitialSetupRecoveryRecord& record
    )
{
    return QJsonObject{
        {QStringLiteral("version"), 1},
        {QStringLiteral("profile"), toQString(record.profile)},
        {QStringLiteral("originalBackup"), toQString(record.originalBackup)}
    };
}

} // namespace

WorkspaceStoreInitialSetupLifecycleAdapter::
WorkspaceStoreInitialSetupLifecycleAdapter(
    Application::WorkspaceCoordinator& workspaceCoordinator,
    Application::WorkspaceState& workspaceState,
    QString recoveryJournalPath
    ) noexcept
    : m_workspaceCoordinator(workspaceCoordinator),
      m_workspaceState(workspaceState),
      m_recoveryJournalPath(std::move(recoveryJournalPath))
{
    if (m_recoveryJournalPath.isEmpty())
    {
        m_recoveryJournalPath = QDir(
            QStandardPaths::writableLocation(
                QStandardPaths::AppLocalDataLocation
                )
            ).filePath(QStringLiteral("initial-setup-recovery.json"));
    }
    m_recoveryJournalPath = QFileInfo(m_recoveryJournalPath).absoluteFilePath();
}

bool WorkspaceStoreInitialSetupLifecycleAdapter::hasActiveWorkspace() const
{
    return m_workspaceState.snapshot().session().has_value();
}

Domain::Result<void>
WorkspaceStoreInitialSetupLifecycleAdapter::closeActiveWorkspace()
{
    if (!hasActiveWorkspace())
    {
        return Domain::Result<void>::success();
    }
    return m_workspaceCoordinator.closeWorkspace();
}

bool WorkspaceStoreInitialSetupLifecycleAdapter::profileExists(
    const Application::WorkspaceLocation& location
    ) const
{
    const QFileInfo info(toQString(location));
    return info.exists() && info.isFile();
}

Application::WorkspaceLocation
WorkspaceStoreInitialSetupLifecycleAdapter::recoveryLocationFor(
    const Application::WorkspaceLocation& profile
    )
{
    const QFileInfo info(toQString(profile));
    if (info.fileName().isEmpty())
    {
        return {};
    }
    const QString recoveryPath = info.absoluteDir().filePath(
        QStringLiteral(".%1.initial-setup-%2.backup")
            .arg(
                info.fileName(),
                QUuid::createUuid().toString(QUuid::WithoutBraces)
                )
        );
    return Application::WorkspaceLocation(toUtf8(recoveryPath));
}

bool WorkspaceStoreInitialSetupLifecycleAdapter::moveProfile(
    const Application::WorkspaceLocation& source,
    const Application::WorkspaceLocation& destination
    )
{
    if (source.empty() || destination.empty())
    {
        return false;
    }
    // QFile::rename refuses to replace an existing destination, preserving
    // both the original and setup profile when either rename cannot proceed.
    return QFile::rename(toQString(source), toQString(destination));
}

bool WorkspaceStoreInitialSetupLifecycleAdapter::removeProfile(
    const Application::WorkspaceLocation& location
    )
{
    return !location.empty() && QFile::remove(toQString(location));
}

Domain::Result<Application::WorkspaceSession>
WorkspaceStoreInitialSetupLifecycleAdapter::createWorkspace(
    const Application::CreateWorkspaceRequest& request
    )
{
    return m_workspaceCoordinator.createWorkspace(request);
}

Domain::Result<Application::WorkspaceSession>
WorkspaceStoreInitialSetupLifecycleAdapter::openWorkspace(
    const Application::OpenWorkspaceRequest& request
    )
{
    return m_workspaceCoordinator.openWorkspace(request);
}

Domain::Result<
    std::optional<Application::InitialSetupRecoveryRecord>>
WorkspaceStoreInitialSetupLifecycleAdapter::pendingRecovery() const
{
    QFile journal(m_recoveryJournalPath);
    if (!journal.exists())
    {
        return Domain::Result<
            std::optional<Application::InitialSetupRecoveryRecord>>::success(
                std::nullopt
                );
    }
    if (!journal.open(QIODevice::ReadOnly))
    {
        return Domain::Result<
            std::optional<Application::InitialSetupRecoveryRecord>>::failure(
                technicalError(
                    QStringLiteral("The setup recovery record could not be read." )
                    )
                );
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        journal.readAll(),
        &parseError
        );
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject())
    {
        return Domain::Result<
            std::optional<Application::InitialSetupRecoveryRecord>>::failure(
                technicalError(
                    QStringLiteral("The setup recovery record is invalid." )
                    )
                );
    }

    const QJsonObject object = document.object();
    const QString profile = object.value(QStringLiteral("profile")).toString();
    const QString backup = object.value(QStringLiteral("originalBackup")).toString();
    if (object.value(QStringLiteral("version")).toInt() != 1
        || profile.trimmed().isEmpty()
        || !isAbsolutePath(profile)
        || !WorkspaceProfilePathPolicy::hasNativeProfileSuffix(profile)
        || (!backup.isEmpty() && !isAbsolutePath(backup)))
    {
        return Domain::Result<
            std::optional<Application::InitialSetupRecoveryRecord>>::failure(
                technicalError(
                    QStringLiteral("The setup recovery record has invalid paths." )
                    )
                );
    }

    if (!backup.isEmpty())
    {
        const bool backupExists = QFileInfo(backup).isFile();
        const bool profileExists = QFileInfo(profile).isFile();
        if (!backupExists && profileExists)
        {
            // The journal is written before the original is moved. If the
            // target is still present and no backup exists, that move did not
            // occur (or a completed setup already removed its backup). The
            // target is the only surviving profile and remains authoritative.
            journal.close();
            static_cast<void>(QFile::remove(m_recoveryJournalPath));
            return Domain::Result<
                std::optional<Application::InitialSetupRecoveryRecord>>::success(
                    std::nullopt
                    );
        }
        if (!backupExists && !profileExists)
        {
            return Domain::Result<
                std::optional<Application::InitialSetupRecoveryRecord>>::failure(
                    technicalError(
                        QStringLiteral("Neither the interrupted profile nor its preserved backup exists." )
                        )
                    );
        }
    }

    return Domain::Result<
        std::optional<Application::InitialSetupRecoveryRecord>>::success(
            Application::InitialSetupRecoveryRecord{
                Application::WorkspaceLocation(toUtf8(profile)),
                Application::WorkspaceLocation(toUtf8(backup))
            }
            );
}

bool WorkspaceStoreInitialSetupLifecycleAdapter::recordPendingRecovery(
    const Application::InitialSetupRecoveryRecord& record
    )
{
    return static_cast<bool>(writeRecoveryRecord(record));
}

void WorkspaceStoreInitialSetupLifecycleAdapter::clearPendingRecovery(
    const Application::InitialSetupRecoveryRecord& record
    ) noexcept
{
    const auto current = pendingRecovery();
    if (!current || !current.value().has_value()
        || current.value().value() != record)
    {
        return;
    }
    static_cast<void>(QFile::remove(m_recoveryJournalPath));
}

Domain::Result<Application::WorkspaceLocation>
WorkspaceStoreInitialSetupLifecycleAdapter::profileLocationForSetupPath(
    const Application::FileDialogPath& path
    )
{
    return WorkspaceProfilePathPolicy::profileLocationForInitialSetupPath(path);
}

const QString&
WorkspaceStoreInitialSetupLifecycleAdapter::recoveryJournalPath() const noexcept
{
    return m_recoveryJournalPath;
}

Domain::Result<void>
WorkspaceStoreInitialSetupLifecycleAdapter::writeRecoveryRecord(
    const Application::InitialSetupRecoveryRecord& record
    ) const
{
    const QString profile = toQString(record.profile);
    const QString backup = toQString(record.originalBackup);
    if (profile.trimmed().isEmpty()
        || !isAbsolutePath(profile)
        || !WorkspaceProfilePathPolicy::hasNativeProfileSuffix(profile)
        || (!backup.isEmpty() && !isAbsolutePath(backup)))
    {
        return Domain::Result<void>::failure(
            invalidInputError(
                QStringLiteral("The setup recovery paths are invalid." )
                )
            );
    }

    const QFileInfo journalInfo(m_recoveryJournalPath);
    if (!QDir().mkpath(journalInfo.absolutePath()))
    {
        return Domain::Result<void>::failure(
            technicalError(
                QStringLiteral("The setup recovery location could not be created." )
                )
            );
    }

    QSaveFile journal(m_recoveryJournalPath);
    if (!journal.open(QIODevice::WriteOnly))
    {
        return Domain::Result<void>::failure(
            technicalError(
                QStringLiteral("The setup recovery record could not be saved." )
                )
            );
    }
    const QByteArray payload = QJsonDocument(recordObject(record)).toJson(
        QJsonDocument::Compact
        );
    if (journal.write(payload) != payload.size() || !journal.commit())
    {
        return Domain::Result<void>::failure(
            technicalError(
                QStringLiteral("The setup recovery record could not be saved." )
                )
            );
    }
    return Domain::Result<void>::success();
}

} // namespace ClassMngr::Next::Platform
