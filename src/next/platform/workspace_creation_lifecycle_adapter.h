#pragma once

#include "core/application_services.h"
#include "next/application/workspace_creation_lifecycle.h"
#include "next/application/workspace_coordinator.h"
#include "next/application/workspace_state.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QUuid>

#include <cstddef>
#include <string>

namespace ClassMngr::Next::Platform
{

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
