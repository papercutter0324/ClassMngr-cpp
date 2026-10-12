#pragma once

#include "core/settingsmanager.h"
#include "next/application/save_mode_preferences.h"
#include "ui/shared/state/option_state_keys.h"

#include <QString>
#include <QVariant>

#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the save-mode read. The canonical options key,
// legacy SettingsManager access, and integer conversion stay here;
// ActionRegistry remains the compatibility writer and UI owner.
class SettingsManagerSaveModePreferencesPort final
    : public Application::SaveModePreferencesPort
{
public:
    explicit SettingsManagerSaveModePreferencesPort(
        SettingsManager::SaveModePreferenceSync synchronize = {}
        )
        : m_synchronize(std::move(synchronize))
    {
    }

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

    [[nodiscard]] Domain::Result<void> write(
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
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Save Mode must be Automatic or Manual.",
                .recoverable = false
            });
        }

        const auto status =
            SettingsManager::instance().writeSaveModePreference(
                storedMode,
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
                    "Unable to save Save Mode preference: QSettings "
                    ) + statusName + ".",
                .recoverable = true
            });
        }

        return Domain::Result<void>::success();
    }

private:
    SettingsManager::SaveModePreferenceSync m_synchronize;

    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            OptionKeys::SaveMode
            );
    }
};

} // namespace ClassMngr::Next::Platform
