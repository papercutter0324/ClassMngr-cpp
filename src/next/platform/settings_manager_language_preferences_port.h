#pragma once

#include "core/settingsmanager.h"
#include "next/application/language_preferences_port.h"

#include <QString>
#include <QVariant>

#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the persisted language preference. The canonical
// option key and legacy 2/3/4 migration stay confined here; application code
// consumes the existing typed LanguagePreference vocabulary.
class SettingsManagerLanguagePreferencesPort final
    : public Application::LanguagePreferencesPort
{
public:
    explicit SettingsManagerLanguagePreferencesPort(
        SettingsManager::LanguagePreferenceSync synchronize = {}
        )
        : m_synchronize(std::move(synchronize))
    {
    }

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

    [[nodiscard]] Domain::Result<void> write(
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
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message =
                    "Language preference must be SystemDefault, English, or Korean.",
                .recoverable = false
            });
        }

        const QSettings::Status status =
            SettingsManager::instance().writeLanguagePreference(
                storedLanguage,
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
                    "Unable to save Language preference: QSettings "
                    ) + statusName + ".",
                .recoverable = true
            });
        }

        return Domain::Result<void>::success();
    }

    void clear() const override
    {
        SettingsManager::instance().remove(key());
    }

private:
    SettingsManager::LanguagePreferenceSync m_synchronize;

    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            SettingsManager::Keys::LANGUAGE_PREFERENCE
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
