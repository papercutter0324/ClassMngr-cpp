#pragma once

#include "core/settingsmanager.h"
#include "next/application/automatic_update_preferences.h"

#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the automatic-update preference. The legacy
// SettingsManager singleton is intentionally confined to this adapter;
// application callers consume only the typed read/write contract.
class SettingsManagerAutomaticUpdatePreferencesPort final
    : public Application::AutomaticUpdatePreferencesPort
{
public:
    SettingsManagerAutomaticUpdatePreferencesPort() = default;

    [[nodiscard]] Application::AutomaticUpdatePreferences read()
        const override
    {
        const SettingsManager& settings = SettingsManager::instance();
        return {
            .automaticChecksEnabled = readEnabled(
                settings.get(
                    QString::fromUtf8(
                        SettingsManager::Keys::AUTOMATIC_UPDATE_CHECKS_ENABLED
                        )
                    )
                )
        };
    }

    void write(
        const Application::AutomaticUpdatePreferences& preferences
        ) const override
    {
        SettingsManager::instance().set(
            QString::fromUtf8(
                SettingsManager::Keys::AUTOMATIC_UPDATE_CHECKS_ENABLED
                ),
            preferences.automaticChecksEnabled
            );
    }

private:
    [[nodiscard]] static bool readEnabled(const QVariant& value)
    {
        // Invalid or unavailable legacy values retain the enabled default.
        // Valid values use QVariant's legacy bool coercion unchanged.
        return value.isValid()
            ? value.toBool()
            : true;
    }
};

} // namespace ClassMngr::Next::Platform
