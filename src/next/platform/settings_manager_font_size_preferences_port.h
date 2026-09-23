#pragma once

#include "core/settingsmanager.h"
#include "next/application/font_size_preferences.h"
#include "ui/shared/state/option_state_keys.h"

#include <QString>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the startup font-size read. The legacy
// SettingsManager singleton and integer conversion stay here; ActionRegistry
// remains the compatibility bridge and menu owner for this option.
class SettingsManagerFontSizePreferencesPort final
    : public Application::FontSizePreferencesPort
{
public:
    SettingsManagerFontSizePreferencesPort() = default;

    [[nodiscard]] Application::FontSize read() const override
    {
        const int storedValue =
            SettingsManager::instance().get(
                key(),
                static_cast<int>(Application::FontSize::Normal)
                ).toInt();

        switch (storedValue)
        {
        case static_cast<int>(Application::FontSize::Small):
            return Application::FontSize::Small;
        case static_cast<int>(Application::FontSize::Normal):
            return Application::FontSize::Normal;
        case static_cast<int>(Application::FontSize::Large):
            return Application::FontSize::Large;
        case static_cast<int>(Application::FontSize::ExtraLarge):
            return Application::FontSize::ExtraLarge;
        default:
            return Application::FontSize::Normal;
        }
    }

    void write(
        const Application::FontSize fontSize
        ) const override
    {
        int storedValue = 0;
        switch (fontSize)
        {
        case Application::FontSize::Small:
            storedValue = -2;
            break;

        case Application::FontSize::Normal:
            storedValue = 0;
            break;

        case Application::FontSize::Large:
            storedValue = 2;
            break;

        case Application::FontSize::ExtraLarge:
            storedValue = 4;
            break;

        default:
            return;
        }

        SettingsManager::instance().set(
            key(),
            storedValue
            );
    }

private:
    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            OptionKeys::FontSize
            );
    }
};

} // namespace ClassMngr::Next::Platform
