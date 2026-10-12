#pragma once

#include "core/settingsmanager.h"
#include "next/application/recent_workspace_history.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the recent workspace history. The legacy
// SettingsManager singleton and its recent/last-file keys are intentionally
// confined here; last-database-directory persistence remains a separate
// FileController concern.
class SettingsManagerRecentWorkspaceHistoryPort final
    : public Application::RecentWorkspaceHistoryPort
{
public:
    explicit SettingsManagerRecentWorkspaceHistoryPort(
        SettingsManager::RecentWorkspaceHistoryPreferencesSync synchronize = {}
        )
        : m_synchronize(std::move(synchronize))
    {
    }

    [[nodiscard]] Application::RecentWorkspaceHistory load()
        const override
    {
        const SettingsManager& settings = SettingsManager::instance();
        Application::RecentWorkspaceHistory history;

        const QStringList storedPaths = settings.getRecentFiles();
        history.paths.reserve(
            static_cast<std::size_t>(storedPaths.size())
            );
        for (const QString& path : storedPaths)
        {
            history.paths.emplace_back(
                Application::RecentWorkspacePath(toUtf8(path))
                );
        }

        const QString lastPath = settings.getLastFile();
        if (!lastPath.isEmpty())
        {
            history.lastPath = Application::RecentWorkspacePath(
                toUtf8(lastPath)
                );
        }

        return history;
    }

    [[nodiscard]] Domain::Result<void> save(
        const Application::RecentWorkspaceHistory& history
        ) const override
    {
        QStringList storedPaths;
        storedPaths.reserve(
            static_cast<qsizetype>(history.paths.size())
            );
        for (const Application::RecentWorkspacePath& path : history.paths)
        {
            storedPaths.append(fromUtf8(path.value()));
        }

        const QSettings::Status status =
            SettingsManager::instance().writeRecentWorkspaceHistoryPreferences(
                storedPaths,
                history.lastPath.has_value()
                    ? fromUtf8(history.lastPath->value())
                    : QString(),
                m_synchronize
                );
        return resultForStatus(status, "save");
    }

    [[nodiscard]] Domain::Result<void> clear() const override
    {
        const QSettings::Status status =
            SettingsManager::instance().writeRecentWorkspaceHistoryPreferences(
                QStringList(),
                QString(),
                m_synchronize
                );
        return resultForStatus(status, "clear");
    }

private:
    [[nodiscard]] static Domain::Result<void> resultForStatus(
        const QSettings::Status status,
        const char* operation
        )
    {
        if (status == QSettings::NoError)
        {
            return Domain::Result<void>::success();
        }

        const char* statusName = status == QSettings::AccessError
            ? "AccessError (settings could not be written)"
            : "FormatError (settings contain invalid data)";
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = std::string(
                "Unable to "
                ) + operation + " recent workspace history: QSettings "
                    + statusName + ".",
            .recoverable = true
        });
    }

    [[nodiscard]] static std::string toUtf8(
        const QString& value
        )
    {
        const QByteArray encoded = value.toUtf8();
        return std::string(
            encoded.constData(),
            static_cast<std::size_t>(encoded.size())
            );
    }

    [[nodiscard]] static QString fromUtf8(
        const std::string& value
        )
    {
        return QString::fromUtf8(
            value.data(),
            static_cast<qsizetype>(value.size())
            );
    }

    SettingsManager::RecentWorkspaceHistoryPreferencesSync m_synchronize;
};

} // namespace ClassMngr::Next::Platform
