#pragma once

#include "core/settingsmanager.h"
#include "next/application/automatic_update_preferences.h"

#include <QString>
#include <QVariant>

#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the automatic-update preference. The legacy
// SettingsManager singleton is intentionally confined to this adapter;
// application callers consume only the typed read/write contract.
class SettingsManagerAutomaticUpdatePreferencesPort final
    : public Application::AutomaticUpdatePreferencesPort
{
public:
    explicit SettingsManagerAutomaticUpdatePreferencesPort(
        SettingsManager::AutomaticUpdatePreferenceSync synchronize = {}
        )
        : m_synchronize(std::move(synchronize))
    {
    }

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

    [[nodiscard]] Domain::Result<void> write(
        const Application::AutomaticUpdatePreferences& preferences
        ) const override
    {
        const QSettings::Status status =
            SettingsManager::instance().writeAutomaticUpdatePreference(
                preferences.automaticChecksEnabled,
                m_synchronize
                );

        if (status != QSettings::NoError)
        {
            const char* statusName = status == QSettings::AccessError
                ? "AccessError (settings could not be written)"
                : "FormatError (settings contain invalid data)";
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = std::string(
                    "Unable to save Automatically Check for Updates "
                    "preference: QSettings "
                    ) + statusName + ".",
                .recoverable = true
            });
        }

        return Domain::Result<void>::success();
    }

private:
    SettingsManager::AutomaticUpdatePreferenceSync m_synchronize;

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
