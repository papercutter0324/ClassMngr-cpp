#pragma once

#include "core/application_services.h"
#include "next/application/initial_setup_lifecycle.h"
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

// Qt and legacy workspace adapter for the initial setup profile lifecycle.
// It delegates workspace semantics to the existing coordinator and confines
// profile path/file operations to this Platform boundary.
class InitialSetupLifecycleAdapter final
    : public Application::InitialSetupLifecyclePort
{
public:
    InitialSetupLifecycleAdapter(
        ApplicationServices& services,
        Application::WorkspaceCoordinator& workspaceCoordinator,
        Application::WorkspaceState& workspaceState
        ) noexcept
        : m_services(services),
          m_workspaceCoordinator(workspaceCoordinator),
          m_workspaceState(workspaceState)
    {
    }

    [[nodiscard]] bool hasActiveWorkspace() const override
    {
        return m_services.hasOpenDatabase()
            || m_workspaceState.snapshot().session().has_value();
    }

    [[nodiscard]] Domain::Result<void> closeActiveWorkspace() override
    {
        if (m_workspaceState.snapshot().session().has_value())
        {
            return m_workspaceCoordinator.closeWorkspace();
        }

        m_services.closeDatabase();
        return Domain::Result<void>::success();
    }

    [[nodiscard]] bool profileExists(
        const Application::WorkspaceLocation& location
        ) const override
    {
        return QFile::exists(toQString(location));
    }

    [[nodiscard]] Application::WorkspaceLocation recoveryLocationFor(
        const Application::WorkspaceLocation& profile
        ) override
    {
        const QFileInfo info(toQString(profile));
        const QString recoveryPath = info.absoluteDir().filePath(
            QStringLiteral(".%1.initial-setup-%2.backup")
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
        // QFile::rename fails when the destination already exists. Keeping
        // that behavior is essential for both original and setup recovery.
        return QFile::rename(toQString(source), toQString(destination));
    }

    [[nodiscard]] bool removeProfile(
        const Application::WorkspaceLocation& location
        ) override
    {
        return QFile::remove(toQString(location));
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
