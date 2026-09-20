#pragma once

#include "core/settingsmanager.h"
#include "next/application/recent_workspace_history.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <string>

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
    SettingsManagerRecentWorkspaceHistoryPort() = default;

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

    void save(
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

        SettingsManager& settings = SettingsManager::instance();
        settings.setRecentFiles(storedPaths);
        settings.setLastFile(
            history.lastPath.has_value()
                ? fromUtf8(history.lastPath->value())
                : QString()
            );
    }

    void clear() const override
    {
        SettingsManager& settings = SettingsManager::instance();
        settings.clearRecentFiles();
        settings.setLastFile(QString());
    }

private:
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
};

} // namespace ClassMngr::Next::Platform
