#include "next/platform/workspace_store_creation_lifecycle_adapter.h"

#include <QDir>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

#include <cstddef>
#include <string>

namespace ClassMngr::Next::Platform
{
namespace
{

[[nodiscard]] QString toQString(const Application::WorkspaceLocation& location)
{
    return QString::fromUtf8(
        location.value().data(),
        static_cast<qsizetype>(location.value().size())
        );
}

[[nodiscard]] Application::WorkspaceLocation fromQString(const QString& path)
{
    const QByteArray encoded = path.toUtf8();
    return Application::WorkspaceLocation(std::string(
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
        ));
}

[[nodiscard]] Domain::OperationError closeFailure()
{
    return Domain::OperationError{
        .code = Domain::ErrorCode::Technical,
        .message = "The failed workspace could not be closed.",
        .recoverable = true
    };
}

} // namespace

bool QtWorkspaceCreationFileOperationsPort::profileExists(
    const Application::WorkspaceLocation& location
    ) const
{
    const QFileInfo info(toQString(location));
    return info.exists() && info.isFile();
}

Application::WorkspaceLocation
QtWorkspaceCreationFileOperationsPort::recoveryLocationFor(
    const Application::WorkspaceLocation& profile
    )
{
    const QFileInfo info(toQString(profile));
    if (info.fileName().isEmpty())
    {
        return {};
    }
    return fromQString(info.absoluteDir().filePath(
        QStringLiteral(".%1.workspace-create-%2.recovery")
            .arg(
                info.fileName(),
                QUuid::createUuid().toString(QUuid::WithoutBraces)
                )
        ));
}

bool QtWorkspaceCreationFileOperationsPort::moveProfile(
    const Application::WorkspaceLocation& source,
    const Application::WorkspaceLocation& destination
    )
{
    if (source.empty() || destination.empty())
    {
        return false;
    }
    // QFile::rename refuses to replace an existing destination, preserving
    // both the profile and any colliding recovery file.
    return QFile::rename(toQString(source), toQString(destination));
}

bool QtWorkspaceCreationFileOperationsPort::removeProfile(
    const Application::WorkspaceLocation& location
    )
{
    return !location.empty() && QFile::remove(toQString(location));
}

bool WorkspaceStoreCreationLifecycleAdapter::profileExists(
    const Application::WorkspaceLocation& location
    ) const
{
    return m_fileOperations.profileExists(location);
}

Application::WorkspaceLocation
WorkspaceStoreCreationLifecycleAdapter::recoveryLocationFor(
    const Application::WorkspaceLocation& profile
    )
{
    return m_fileOperations.recoveryLocationFor(profile);
}

bool WorkspaceStoreCreationLifecycleAdapter::moveProfile(
    const Application::WorkspaceLocation& source,
    const Application::WorkspaceLocation& destination
    )
{
    return m_fileOperations.moveProfile(source, destination);
}

bool WorkspaceStoreCreationLifecycleAdapter::removeProfile(
    const Application::WorkspaceLocation& location
    )
{
    return m_fileOperations.removeProfile(location);
}

Domain::Result<void>
WorkspaceStoreCreationLifecycleAdapter::closeActiveWorkspace()
{
    if (!m_workspaceState.snapshot().session().has_value())
    {
        return Domain::Result<void>::success();
    }

    const Domain::Result<void> closed = m_workspaceCoordinator.closeWorkspace();
    if (closed || !m_workspaceState.snapshot().session().has_value())
    {
        return closed;
    }
    return Domain::Result<void>::failure(closeFailure());
}

Domain::Result<Application::WorkspaceSession>
WorkspaceStoreCreationLifecycleAdapter::createWorkspace(
    const Application::CreateWorkspaceRequest& request
    )
{
    return m_workspaceCoordinator.createWorkspace(request);
}

} // namespace ClassMngr::Next::Platform
