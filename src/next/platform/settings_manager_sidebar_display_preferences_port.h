#pragma once

#include "core/settingsmanager.h"
#include "next/application/sidebar_display_preferences.h"

#include <QString>
#include <QVariant>

#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the bounded sidebar display preferences. The
// legacy SettingsManager singleton is intentionally confined to this adapter;
// application callers consume only the typed load/save contract.
class SettingsManagerSidebarDisplayPreferencesPort final
    : public Application::SidebarDisplayPreferencesPort
{
public:
    explicit SettingsManagerSidebarDisplayPreferencesPort(
        SettingsManager::SidebarDisplayPreferencesSync synchronize = {}
        )
        : m_synchronize(std::move(synchronize))
    {
    }

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
        const QSettings::Status status =
            SettingsManager::instance().writeSidebarDisplayPreferences(
                request.sidebarTooltipsEnabled,
                request.sidebarMarqueeEnabled,
                m_synchronize
                );
        if (status != QSettings::NoError)
        {
            const char* statusName = status == QSettings::AccessError
                ? "AccessError (settings could not be written)"
                : "FormatError (settings contain invalid data)";
            return Application::SidebarDisplayPreferencesSaveResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = std::string(
                    "Unable to save Sidebar Display preferences: QSettings "
                    ) + statusName + ".",
                .recoverable = true
            });
        }

        return Application::SidebarDisplayPreferencesSaveResult::success();
    }

private:
    SettingsManager::SidebarDisplayPreferencesSync m_synchronize;

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
