#pragma once

#include "core/settingsmanager.h"
#include "next/application/skipped_update_version_preferences.h"

#include <QString>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the skipped update version. The legacy
// SettingsManager singleton is intentionally confined to this adapter;
// application callers consume only the typed read/write/clear contract.
class SettingsManagerSkippedUpdateVersionPort final
    : public Application::SkippedUpdateVersionPreferencesPort
{
public:
    SettingsManagerSkippedUpdateVersionPort() = default;

    [[nodiscard]] Application::SkippedUpdateVersionPreferences read()
        const override
    {
        const QString storedVersion =
            SettingsManager::instance().get(
                QString::fromUtf8(
                    SettingsManager::Keys::SKIPPED_UPDATE_VERSION
                    ),
                QString()
                ).toString().trimmed();

        if (storedVersion.isEmpty())
        {
            return {};
        }

        return {
            .skippedVersion = storedVersion.toStdString()
        };
    }

    void write(
        const Application::SkippedUpdateVersionPreferences& preferences
        ) const override
    {
        const QString version = preferences.skippedVersion.has_value()
            ? QString::fromStdString(*preferences.skippedVersion).trimmed()
            : QString();

        SettingsManager::instance().set(
            QString::fromUtf8(
                SettingsManager::Keys::SKIPPED_UPDATE_VERSION
                ),
            version
            );
    }

    void clear() const override
    {
        SettingsManager::instance().remove(
            QString::fromUtf8(
                SettingsManager::Keys::SKIPPED_UPDATE_VERSION
                )
            );
    }
};

} // namespace ClassMngr::Next::Platform
