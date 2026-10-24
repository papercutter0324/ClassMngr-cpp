#pragma once

#include "core/application_services.h"
#include "next/application/workspace_creation_lifecycle.h"
#include "next/application/workspace_coordinator.h"
#include "next/application/workspace_state.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QString>
#include <QVector>
#include <QUuid>

#include <cstddef>
#include <string>

namespace ClassMngr::Next::Platform
{

namespace
{
inline const QRegularExpression& normalCreateRecoveryNamePattern()
{
    static const QRegularExpression pattern(
        QStringLiteral(
            "^\\.([\\s\\S]+)\\.workspace-create-"
            "([0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-"
            "[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12})\\.recovery$"
            )
        );
    return pattern;
}
}

struct NormalWorkspaceRecoveryCandidate final
{
    QString sourcePath;
    QString targetPath;
};

// Qt and legacy adapter for the normal profile creation replacement policy.
// It confines sibling file recovery operations to Platform and delegates
// workspace/session behavior to WorkspaceCoordinator.
class WorkspaceCreationLifecycleAdapter final
    : public Application::WorkspaceCreationLifecyclePort
{
public:
    WorkspaceCreationLifecycleAdapter(
        ApplicationServices& services,
        Application::WorkspaceCoordinator& workspaceCoordinator,
        Application::WorkspaceState& workspaceState
        ) noexcept
        : m_services(services),
          m_workspaceCoordinator(workspaceCoordinator),
          m_workspaceState(workspaceState)
    {
    }

    [[nodiscard]] bool profileExists(
        const Application::WorkspaceLocation& location
        ) const override
    {
        return QFileInfo::exists(toQString(location));
    }

    [[nodiscard]] Application::WorkspaceLocation recoveryLocationFor(
        const Application::WorkspaceLocation& profile
        ) override
    {
        const QFileInfo info(toQString(profile));
        const QString recoveryPath = info.absoluteDir().filePath(
            QStringLiteral(".%1.workspace-create-%2.recovery")
                .arg(
                    info.fileName(),
                    QUuid::createUuid().toString(QUuid::WithoutBraces)
                    )
            );
        return fromQString(recoveryPath);
    }

    [[nodiscard]] QVector<NormalWorkspaceRecoveryCandidate>
    normalCreateRecoveryCandidatesIn(
        const QString& directoryPath
        ) const
    {
        QVector<NormalWorkspaceRecoveryCandidate> candidates;
        const QDir directory(directoryPath);
        const QFileInfoList entries = directory.entryInfoList(
            QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
            QDir::Name
            );
        candidates.reserve(entries.size());

        for (const QFileInfo& entry : entries)
        {
            if (!entry.isFile() || entry.isSymLink())
            {
                continue;
            }

            const QRegularExpressionMatch match =
                normalCreateRecoveryNamePattern().match(entry.fileName());
            if (!match.hasMatch())
            {
                continue;
            }

            candidates.append(
                NormalWorkspaceRecoveryCandidate{
                    .sourcePath = entry.absoluteFilePath(),
                    .targetPath = directory.filePath(match.captured(1))
                }
                );
        }

        return candidates;
    }

    // NewOnly refuses an existing destination, so a collision cannot replace
    // either a profile or another recovery copy.
    [[nodiscard]] bool copyRecoveryCandidateNoReplace(
        const QString& sourcePath,
        const QString& destinationPath
        ) const
    {
        const QFileInfo sourceInfo(sourcePath);
        if (
            sourcePath.trimmed().isEmpty()
            || destinationPath.trimmed().isEmpty()
            || !sourceInfo.isFile()
            || sourceInfo.isSymLink()
            || !normalCreateRecoveryNamePattern()
                    .match(sourceInfo.fileName())
                    .hasMatch()
        )
        {
            return false;
        }

        QFile source(sourcePath);
        if (!source.open(QIODevice::ReadOnly))
        {
            return false;
        }

        // NewOnly gives the destination an atomic no-replace create. Keep a
        // partial destination on any later read, write, flush, or permission
        // failure so recovery never hides an artifact it created.
        QFile destination(destinationPath);
        if (!destination.open(QIODevice::WriteOnly | QIODevice::NewOnly))
        {
            return false;
        }

        QByteArray buffer(64 * 1024, '\0');
        while (true)
        {
            const qint64 bytesRead = source.read(
                buffer.data(),
                buffer.size()
                );
            if (bytesRead < 0)
            {
                return false;
            }
            if (bytesRead == 0)
            {
                if (source.error() != QFileDevice::NoError)
                {
                    return false;
                }
                break;
            }

            qint64 bytesWritten = 0;
            while (bytesWritten < bytesRead)
            {
                const qint64 writeResult = destination.write(
                    buffer.constData() + bytesWritten,
                    bytesRead - bytesWritten
                    );
                if (writeResult <= 0)
                {
                    return false;
                }
                bytesWritten += writeResult;
            }
        }

        return destination.flush()
            && destination.setPermissions(source.permissions());
    }

    [[nodiscard]] bool moveProfile(
        const Application::WorkspaceLocation& source,
        const Application::WorkspaceLocation& destination
        ) override
    {
        // QFile::rename does not replace an existing destination. That
        // preserves unrelated files if a recovery name collides.
        return QFile::rename(toQString(source), toQString(destination));
    }

    [[nodiscard]] bool removeProfile(
        const Application::WorkspaceLocation& location
        ) override
    {
        return QFile::remove(toQString(location));
    }

    [[nodiscard]] Domain::Result<void> closeActiveWorkspace() override
    {
        if (m_workspaceState.snapshot().session().has_value())
        {
            return m_workspaceCoordinator.closeWorkspace();
        }

        m_services.closeDatabase();
        if (m_services.hasOpenDatabase())
        {
            return Domain::Result<void>::failure(
                Domain::OperationError{
                    .code = Domain::ErrorCode::Technical,
                    .message = "The failed workspace could not be closed.",
                    .recoverable = true
                }
                );
        }

        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<Application::WorkspaceSession>
    createWorkspace(
        const Application::CreateWorkspaceRequest& request
        ) override
    {
        return m_workspaceCoordinator.createWorkspace(request);
    }

private:
    [[nodiscard]] static QString toQString(
        const Application::WorkspaceLocation& location
        )
    {
        return QString::fromUtf8(
            location.value().data(),
            static_cast<qsizetype>(location.value().size())
            );
    }

    [[nodiscard]] static Application::WorkspaceLocation fromQString(
        const QString& value
        )
    {
        const QByteArray encoded = value.toUtf8();
        return Application::WorkspaceLocation(
            std::string(
                encoded.constData(),
                static_cast<std::size_t>(encoded.size())
                )
            );
    }

    ApplicationServices& m_services;
    Application::WorkspaceCoordinator& m_workspaceCoordinator;
    Application::WorkspaceState& m_workspaceState;
};

} // namespace ClassMngr::Next::Platform
