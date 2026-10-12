#pragma once

#include "core/settingsmanager.h"
#include "next/application/font_size_preferences.h"
#include "ui/shared/state/option_state_keys.h"

#include <QString>

#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the font-size preference. The legacy SettingsManager
// singleton and integer conversion stay here; ActionRegistry remains the
// compatibility bridge and menu owner for this option.
class SettingsManagerFontSizePreferencesPort final
    : public Application::FontSizePreferencesPort
{
public:
    explicit SettingsManagerFontSizePreferencesPort(
        SettingsManager::FontSizePreferenceSync synchronize = {}
        )
        : m_synchronize(std::move(synchronize))
    {
    }

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

    [[nodiscard]] Domain::Result<void> write(
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
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message =
                    "Font Size must be Small, Normal, Large, or Extra Large.",
                .recoverable = false
            });
        }

        const QSettings::Status status =
            SettingsManager::instance().writeFontSizePreference(
                storedValue,
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
                    "Unable to save Font Size preference: QSettings "
                    ) + statusName + ".",
                .recoverable = true
            });
        }

        return Domain::Result<void>::success();
    }

private:
    SettingsManager::FontSizePreferenceSync m_synchronize;

    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            OptionKeys::FontSize
            );
    }
};

} // namespace ClassMngr::Next::Platform
