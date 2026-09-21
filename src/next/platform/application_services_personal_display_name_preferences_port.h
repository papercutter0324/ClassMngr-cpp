#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/personal_display_name_preferences.h"

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <string>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the personal display name. The exact legacy key,
// QVariant/UTF-8 conversion, and SettingsService failure mapping stay here;
// personal-details writers remain compatibility owners.
class ApplicationServicesPersonalDisplayNamePreferencesPort final
    : public Application::PersonalDisplayNamePreferencesPort
{
public:
    explicit ApplicationServicesPersonalDisplayNamePreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_settingsService(services.settingsService())
    {
    }

    explicit ApplicationServicesPersonalDisplayNamePreferencesPort(
        SettingsService* settingsService
        ) noexcept
        : m_settingsService(settingsService)
    {
    }

    ApplicationServicesPersonalDisplayNamePreferencesPort(
        const ApplicationServicesPersonalDisplayNamePreferencesPort&
        ) = delete;
    ApplicationServicesPersonalDisplayNamePreferencesPort& operator=(
        const ApplicationServicesPersonalDisplayNamePreferencesPort&
        ) = delete;
    ApplicationServicesPersonalDisplayNamePreferencesPort(
        ApplicationServicesPersonalDisplayNamePreferencesPort&&
        ) = delete;
    ApplicationServicesPersonalDisplayNamePreferencesPort& operator=(
        ApplicationServicesPersonalDisplayNamePreferencesPort&&
        ) = delete;

    [[nodiscard]] std::string read() const override
    {
        if (!m_settingsService || !m_settingsService->isAvailable())
        {
            return {};
        }

        const QByteArray storedName =
            m_settingsService
                ->loadOrDefault(
                    key(),
                    QString()
                    )
                .toString()
                .toUtf8();

        return std::string(
            storedName.constData(),
            static_cast<std::size_t>(storedName.size())
            );
    }

    [[nodiscard]] Application::PersonalDisplayNamePreferencesSaveResult
    write(
        const std::string& name
        ) const override
    {
        if (!m_settingsService || !m_settingsService->isAvailable())
        {
            return Application::
                PersonalDisplayNamePreferencesSaveResult::success();
        }

        const Status saved = m_settingsService->save(
            key(),
            fromUtf8(name)
            );
        if (!saved)
        {
            const QByteArray errorBytes = saved.error().toUtf8();
            return Application::
                PersonalDisplayNamePreferencesSaveResult::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = errorBytes.isEmpty()
                        ? "Personal display name could not be saved."
                        : errorBytes.toStdString(),
                    .recoverable = false
                });
        }

        return Application::
            PersonalDisplayNamePreferencesSaveResult::success();
    }

private:
    [[nodiscard]] static QString key()
    {
        return QStringLiteral("myInfo/name");
    }

    [[nodiscard]] static QString fromUtf8(
        const std::string& value
        )
    {
        return QString::fromUtf8(
            value.data(),
            static_cast<qsizetype>(value.size())
            );
    }

    SettingsService* m_settingsService = nullptr;
};

} // namespace ClassMngr::Next::Platform
