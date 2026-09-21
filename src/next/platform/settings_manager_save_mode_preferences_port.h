#pragma once

#include "core/settingsmanager.h"
#include "next/application/save_mode_preferences.h"
#include "ui/shared/state/option_state_keys.h"

#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the save-mode read. The canonical options key,
// legacy SettingsManager access, and integer conversion stay here;
// ActionRegistry remains the compatibility writer and UI owner.
class SettingsManagerSaveModePreferencesPort final
    : public Application::SaveModePreferencesPort
{
public:
    SettingsManagerSaveModePreferencesPort() = default;

    [[nodiscard]] Application::SaveMode read() const override
    {
        const QVariant storedValue =
            SettingsManager::instance().get(key());
        bool conversionSucceeded = false;
        const int storedMode = storedValue.toInt(&conversionSucceeded);

        if (!conversionSucceeded)
        {
            return Application::SaveMode::Automatic;
        }

        switch (storedMode)
        {
        case static_cast<int>(Application::SaveMode::Automatic):
            return Application::SaveMode::Automatic;
        case static_cast<int>(Application::SaveMode::Manual):
            return Application::SaveMode::Manual;
        default:
            return Application::SaveMode::Automatic;
        }
    }

    void write(
        const Application::SaveMode mode
        ) const override
    {
        int storedMode = 0;
        switch (mode)
        {
        case Application::SaveMode::Automatic:
            storedMode = 0;
            break;

        case Application::SaveMode::Manual:
            storedMode = 1;
            break;

        default:
            return;
        }

        SettingsManager::instance().set(
            key(),
            storedMode
            );
    }

private:
    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            OptionKeys::SaveMode
            );
    }
};

} // namespace ClassMngr::Next::Platform
