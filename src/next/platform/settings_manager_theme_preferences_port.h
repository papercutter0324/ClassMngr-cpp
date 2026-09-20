#pragma once

#include "core/settingsmanager.h"
#include "next/application/theme_preferences_port.h"
#include "ui/shared/state/option_state_keys.h"

#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the startup theme read. The canonical options key,
// legacy SettingsManager access, and integer conversion stay here.
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

private:
    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            OptionKeys::Theme
            );
    }
};

} // namespace ClassMngr::Next::Platform
