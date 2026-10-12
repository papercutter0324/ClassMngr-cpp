#pragma once

#include "core/settingsmanager.h"
#include "next/application/skipped_update_version_preferences.h"

#include <QString>

#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the skipped update version. The legacy
// SettingsManager singleton is intentionally confined to this adapter;
// application callers consume only the typed read/write/clear contract.
class SettingsManagerSkippedUpdateVersionPort final
    : public Application::SkippedUpdateVersionPreferencesPort
{
public:
    explicit SettingsManagerSkippedUpdateVersionPort(
        SettingsManager::SkippedUpdateVersionPreferenceSync synchronize = {}
        )
        : m_synchronize(std::move(synchronize))
    {
    }

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

    [[nodiscard]] Domain::Result<void> write(
        const Application::SkippedUpdateVersionPreferences& preferences
        ) const override
    {
        const QString version = preferences.skippedVersion.has_value()
            ? QString::fromStdString(*preferences.skippedVersion).trimmed()
            : QString();

        const QSettings::Status status =
            SettingsManager::instance().writeSkippedUpdateVersionPreference(
                version,
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
                    "Unable to save skipped update version preference: "
                    "QSettings "
                    ) + statusName + ".",
                .recoverable = true
            });
        }

        return Domain::Result<void>::success();
    }

    void clear() const override
    {
        SettingsManager::instance().remove(
            QString::fromUtf8(
                SettingsManager::Keys::SKIPPED_UPDATE_VERSION
                )
            );
    }

private:
    SettingsManager::SkippedUpdateVersionPreferenceSync m_synchronize;
};

} // namespace ClassMngr::Next::Platform
