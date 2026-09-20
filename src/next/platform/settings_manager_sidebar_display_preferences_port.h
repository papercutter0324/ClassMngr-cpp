#pragma once

#include "core/settingsmanager.h"
#include "next/application/sidebar_display_preferences.h"

#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the bounded sidebar display preferences. The
// legacy SettingsManager singleton is intentionally confined to this adapter;
// application callers consume only the typed load/save contract.
class SettingsManagerSidebarDisplayPreferencesPort final
    : public Application::SidebarDisplayPreferencesPort
{
public:
    SettingsManagerSidebarDisplayPreferencesPort() = default;

    [[nodiscard]] Application::SidebarDisplayPreferencesResult load()
        const override
    {
        const SettingsManager& settings = SettingsManager::instance();
        return Application::SidebarDisplayPreferencesResult::success({
            .sidebarTooltipsEnabled = readEnabled(
                settings.get(
                    QString::fromUtf8(
                        SettingsManager::Keys::SIDEBAR_TOOLTIPS_ENABLED
                        )
                    )
                ),
            .sidebarMarqueeEnabled = readEnabled(
                settings.get(
                    QString::fromUtf8(
                        SettingsManager::Keys::SIDEBAR_MARQUEE_ENABLED
                        )
                    )
                )
        });
    }

    [[nodiscard]] Application::SidebarDisplayPreferencesSaveResult save(
        const Application::SidebarDisplayPreferencesSaveRequest& request
        ) override
    {
        SettingsManager& settings = SettingsManager::instance();
        settings.set(
            QString::fromUtf8(
                SettingsManager::Keys::SIDEBAR_TOOLTIPS_ENABLED
                ),
            request.sidebarTooltipsEnabled
            );
        settings.set(
            QString::fromUtf8(
                SettingsManager::Keys::SIDEBAR_MARQUEE_ENABLED
                ),
            request.sidebarMarqueeEnabled
            );

        // The legacy SettingsManager API is void and therefore exposes no
        // persistence failure to translate into the typed result.
        return Application::SidebarDisplayPreferencesSaveResult::success();
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
