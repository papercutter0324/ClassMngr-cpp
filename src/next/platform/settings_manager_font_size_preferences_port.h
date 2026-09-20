#pragma once

#include "core/settingsmanager.h"
#include "next/application/font_size_preferences.h"
#include "ui/shared/state/option_state_keys.h"

#include <QString>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the startup font-size read. The legacy
// SettingsManager singleton and integer conversion stay here; ActionRegistry
// remains the compatibility reader/writer and menu owner for this option.
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

private:
    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            OptionKeys::FontSize
            );
    }
};

} // namespace ClassMngr::Next::Platform
