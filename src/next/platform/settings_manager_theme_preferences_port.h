#pragma once

#include "core/settingsmanager.h"
#include "next/application/theme_preferences_port.h"
#include "ui/shared/state/option_state_keys.h"

#include <QString>
#include <QVariant>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for theme preference reads and writes. The canonical
// options key, legacy SettingsManager access, and integer conversion stay here.
class SettingsManagerThemePreferencesPort final
    : public Application::ThemePreferencesPort
{
public:
    explicit SettingsManagerThemePreferencesPort(
        SettingsManager::ThemePreferenceSync synchronize = {}
        )
        : m_synchronize(std::move(synchronize))
    {
    }

    [[nodiscard]] Application::Theme read() const override
    {
        const QVariant storedValue =
            SettingsManager::instance().get(key());
        bool conversionSucceeded = false;
        const int storedTheme =
            storedValue.toInt(&conversionSucceeded);

        if (!conversionSucceeded)
        {
            return Application::Theme::SystemDefault;
        }

        switch (storedTheme)
        {
        case static_cast<int>(Application::Theme::Dark):
            return Application::Theme::Dark;
        case static_cast<int>(Application::Theme::Light):
            return Application::Theme::Light;
        case static_cast<int>(Application::Theme::SystemDefault):
            return Application::Theme::SystemDefault;
        default:
            return Application::Theme::SystemDefault;
        }
    }

    [[nodiscard]] Domain::Result<void> write(
        const Application::Theme theme
        ) const override
    {
        int storedTheme = 0;
        switch (theme)
        {
        case Application::Theme::Dark:
            storedTheme = 0;
            break;

        case Application::Theme::Light:
            storedTheme = 1;
            break;

        case Application::Theme::SystemDefault:
            storedTheme = 2;
            break;

        default:
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Theme must be Dark, Light, or SystemDefault.",
                .recoverable = false
            });
        }

        const auto status = SettingsManager::instance().writeThemePreference(
            storedTheme,
            m_synchronize
            );
        if (status != QSettings::NoError)
        {
            const char* statusName = status == QSettings::AccessError
                ? "AccessError (settings could not be written)"
                : "FormatError (settings contain invalid data)";
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = std::string("Unable to save theme preference: QSettings ")
                    + statusName + ".",
                .recoverable = true
            });
        }
        return Domain::Result<void>::success();
    }

private:
    SettingsManager::ThemePreferenceSync m_synchronize;

    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            OptionKeys::Theme
            );
    }
};

} // namespace ClassMngr::Next::Platform
