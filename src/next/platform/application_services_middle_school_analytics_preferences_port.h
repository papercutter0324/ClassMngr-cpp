#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/middle_school_analytics_preferences.h"

#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the middle-school analytics visibility preference.
// The legacy ApplicationServices/settings access and QVariant conversion stay
// here; callers consume only the typed application boolean.
class ApplicationServicesMiddleSchoolAnalyticsPreferencesPort final
    : public Application::MiddleSchoolAnalyticsPreferencesPort
{
public:
    explicit ApplicationServicesMiddleSchoolAnalyticsPreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort(
        const ApplicationServicesMiddleSchoolAnalyticsPreferencesPort&
        ) = delete;
    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort& operator=(
        const ApplicationServicesMiddleSchoolAnalyticsPreferencesPort&
        ) = delete;
    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort(
        ApplicationServicesMiddleSchoolAnalyticsPreferencesPort&&
        ) = delete;
    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort& operator=(
        ApplicationServicesMiddleSchoolAnalyticsPreferencesPort&&
        ) = delete;

    [[nodiscard]] bool load() const override
    {
        const SettingsService* settingsService = m_services.settingsService();
        if (!settingsService || !settingsService->isAvailable())
        {
            return false;
        }

        const auto storedValue = settingsService->load(key());
        if (!storedValue || !storedValue->isValid())
        {
            // Preserve the legacy preference boundary's default materializing
            // behavior when storage is available.
            static_cast<void>(settingsService->save(key(), false));
            return false;
        }

        // Preserve the legacy ClassNavigationPreferences QVariant coercion.
        return storedValue->toBool();
    }

    void save(const bool show) const override
    {
        const SettingsService* settingsService = m_services.settingsService();
        if (!settingsService || !settingsService->isAvailable())
        {
            return;
        }

        static_cast<void>(settingsService->save(key(), show));
    }

private:
    [[nodiscard]] static QString key()
    {
        return QStringLiteral(
            "classes_navigation_show_middle_school_analytics_and_evaluations"
            );
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
