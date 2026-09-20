#pragma once

#include "core/settingsmanager.h"
#include "next/application/powerpoint_data_access_notice_preferences.h"

#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the PowerPoint data-access notice preference. The
// legacy SettingsManager singleton is intentionally confined to this adapter;
// application callers consume only the typed read/write contract.
class SettingsManagerPowerPointDataAccessNoticePort final
    : public Application::PowerPointDataAccessNoticePreferencesPort
{
public:
    SettingsManagerPowerPointDataAccessNoticePort() = default;

    [[nodiscard]] Application::PowerPointDataAccessNoticePreferences read()
        const override
    {
        const SettingsManager& settings = SettingsManager::instance();
        return {
            .showPowerPointDataAccessNotice = readEnabled(
                settings.get(
                    QString::fromUtf8(
                        SettingsManager::Keys::
                            SHOW_POWERPOINT_DATA_ACCESS_NOTICE
                        )
                    )
                )
        };
    }

    void write(
        const Application::PowerPointDataAccessNoticePreferences& preferences
        ) const override
    {
        SettingsManager::instance().set(
            QString::fromUtf8(
                SettingsManager::Keys::SHOW_POWERPOINT_DATA_ACCESS_NOTICE
                ),
            preferences.showPowerPointDataAccessNotice
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
