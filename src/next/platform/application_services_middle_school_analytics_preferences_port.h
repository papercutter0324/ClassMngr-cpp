#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/middle_school_analytics_preferences.h"

#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the middle-school analytics visibility preference.
// The active-session settings access and QVariant conversion stay here;
// callers consume only the typed application boolean.
class ApplicationServicesMiddleSchoolAnalyticsPreferencesPort final
    : public Application::MiddleSchoolAnalyticsPreferencesPort
{
public:
    explicit ApplicationServicesMiddleSchoolAnalyticsPreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_session(services.databaseSession())
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
        if (!m_session || !m_session->isOpen())
        {
            return false;
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return false;
        }

        const auto storedValue = repository->loadSetting(key());
        if (!storedValue || !storedValue->isValid())
        {
            // Preserve the legacy preference boundary's default materializing
            // behavior when storage is available.
            static_cast<void>(repository->saveSetting(key(), false));
            return false;
        }

        // Preserve the previous QVariant coercion for stored values.
        return storedValue->toBool();
    }

    void save(const bool show) const override
    {
        if (!m_session || !m_session->isOpen())
        {
            return;
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return;
        }

        static_cast<void>(repository->saveSetting(key(), show));
    }

private:
    [[nodiscard]] static QString key()
    {
        return QStringLiteral(
            "classes_navigation_show_middle_school_analytics_and_evaluations"
            );
    }

    DatabaseSession* m_session = nullptr;
};

} // namespace ClassMngr::Next::Platform
