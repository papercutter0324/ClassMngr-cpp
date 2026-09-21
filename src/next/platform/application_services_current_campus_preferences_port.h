#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/current_campus_preferences.h"

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <string>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the read-only current-campus preference. The exact
// legacy key and QVariant-to-UTF-8 conversion stay here; PersonalDetails
// remains the compatibility writer.
class ApplicationServicesCurrentCampusPreferencesPort final
    : public Application::CurrentCampusPreferencesPort
{
public:
    explicit ApplicationServicesCurrentCampusPreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_settingsService(services.settingsService())
    {
    }

    explicit ApplicationServicesCurrentCampusPreferencesPort(
        SettingsService* settingsService
        ) noexcept
        : m_settingsService(settingsService)
    {
    }

    ApplicationServicesCurrentCampusPreferencesPort(
        const ApplicationServicesCurrentCampusPreferencesPort&
        ) = delete;
    ApplicationServicesCurrentCampusPreferencesPort& operator=(
        const ApplicationServicesCurrentCampusPreferencesPort&
        ) = delete;
    ApplicationServicesCurrentCampusPreferencesPort(
        ApplicationServicesCurrentCampusPreferencesPort&&
        ) = delete;
    ApplicationServicesCurrentCampusPreferencesPort& operator=(
        ApplicationServicesCurrentCampusPreferencesPort&&
        ) = delete;

    [[nodiscard]] std::string read() const override
    {
        if (!m_settingsService || !m_settingsService->isAvailable())
        {
            return {};
        }

        const QByteArray storedCampus =
            m_settingsService
                ->loadOrDefault(
                    key(),
                    QString()
                    )
                .toString()
                .toUtf8();

        return std::string(
            storedCampus.constData(),
            static_cast<std::size_t>(storedCampus.size())
            );
    }

private:
    [[nodiscard]] static QString key()
    {
        return QStringLiteral("myInfo/campus");
    }

    SettingsService* m_settingsService = nullptr;
};

} // namespace ClassMngr::Next::Platform
