#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/schedule_display_preferences.h"

#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the read-only schedule display preference. The
// caller owns ApplicationServices; missing or unavailable legacy settings
// preserve the existing false fallback.
class ApplicationServicesScheduleDisplayPreferencesPort final
    : public Application::ScheduleDisplayPreferencesPort
{
public:
    explicit ApplicationServicesScheduleDisplayPreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesScheduleDisplayPreferencesPort(
        const ApplicationServicesScheduleDisplayPreferencesPort&
        ) = delete;
    ApplicationServicesScheduleDisplayPreferencesPort& operator=(
        const ApplicationServicesScheduleDisplayPreferencesPort&
        ) = delete;
    ApplicationServicesScheduleDisplayPreferencesPort(
        ApplicationServicesScheduleDisplayPreferencesPort&&
        ) = delete;
    ApplicationServicesScheduleDisplayPreferencesPort& operator=(
        ApplicationServicesScheduleDisplayPreferencesPort&&
        ) = delete;

    [[nodiscard]] Application::ScheduleDisplayPreferencesResult load()
        const override
    {
        const SettingsService* settingsService = m_services.settingsService();
        if (!settingsService || !settingsService->isAvailable())
        {
            return Application::ScheduleDisplayPreferencesResult::success({});
        }

        return Application::ScheduleDisplayPreferencesResult::success({
            .use24HourTime = settingToBool(
                settingsService->loadOrDefault(
                    QStringLiteral("schedule_use_24h"),
                    false
                    ),
                false
                )
        });
    }

private:
    [[nodiscard]] static bool settingToBool(
        const QVariant& value,
        const bool defaultValue
        )
    {
        if (!value.isValid())
        {
            return defaultValue;
        }

        const QString text =
            value.toString().trimmed().toLower();

        if (text == QStringLiteral("true") || text == QStringLiteral("1"))
        {
            return true;
        }

        if (text == QStringLiteral("false") || text == QStringLiteral("0"))
        {
            return false;
        }

        return value.toBool();
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
