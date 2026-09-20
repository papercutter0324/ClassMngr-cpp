#pragma once

#include "core/settingsmanager.h"
#include "next/application/language_preferences_port.h"
#include "ui/shared/state/option_state_keys.h"

#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the persisted language preference. The canonical
// option key and legacy 2/3/4 migration stay confined here; application code
// consumes the existing typed LanguagePreference vocabulary.
class SettingsManagerLanguagePreferencesPort final
    : public Application::LanguagePreferencesPort
{
public:
    SettingsManagerLanguagePreferencesPort() = default;

    [[nodiscard]] Application::LanguagePreference read() const override
    {
        const QVariant storedValue =
            SettingsManager::instance().get(
                key(),
                static_cast<int>(
                    Application::LanguagePreference::SystemDefault
                    )
                );
        bool conversionSucceeded = false;
        const int storedLanguage =
            storedValue.toInt(&conversionSucceeded);

        if (!conversionSucceeded)
        {
            return Application::LanguagePreference::SystemDefault;
        }

        switch (storedLanguage)
        {
        case static_cast<int>(
            Application::LanguagePreference::SystemDefault
            ):
            return Application::LanguagePreference::SystemDefault;

        case static_cast<int>(Application::LanguagePreference::English):
        case 2:
        case 3:
        case 4:
            migrateToEnglish();
            return Application::LanguagePreference::English;

        case 5:
            return Application::LanguagePreference::Korean;

        default:
            return Application::LanguagePreference::SystemDefault;
        }
    }

    void write(
        const Application::LanguagePreference preference
        ) const override
    {
        int storedLanguage = 0;
        switch (preference)
        {
        case Application::LanguagePreference::SystemDefault:
            storedLanguage = 0;
            break;

        case Application::LanguagePreference::English:
            storedLanguage = 1;
            break;

        case Application::LanguagePreference::Korean:
            storedLanguage = 5;
            break;

        default:
            return;
        }

        SettingsManager::instance().set(key(), storedLanguage);
    }

    void clear() const override
    {
        SettingsManager::instance().remove(key());
    }

private:
    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            OptionKeys::Language
            );
    }

    static void migrateToEnglish()
    {
        SettingsManager::instance().set(
            key(),
            static_cast<int>(
                Application::LanguagePreference::English
                )
            );
    }
};

} // namespace ClassMngr::Next::Platform
