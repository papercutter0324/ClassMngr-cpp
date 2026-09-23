#pragma once

#include "core/settingsmanager.h"
#include "next/application/theme_preferences_port.h"
#include "ui/shared/state/option_state_keys.h"

#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for theme preference reads and writes. The canonical
// options key, legacy SettingsManager access, and integer conversion stay here.
class SettingsManagerThemePreferencesPort final
    : public Application::ThemePreferencesPort
{
public:
    SettingsManagerThemePreferencesPort() = default;

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

    void write(
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
            return;
        }

        SettingsManager::instance().set(
            key(),
            storedTheme
            );
    }

private:
    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            OptionKeys::Theme
            );
    }
};

} // namespace ClassMngr::Next::Platform
