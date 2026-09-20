#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/class_visibility_preferences.h"

#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the class visibility scope. The legacy
// ApplicationServices/settings access and QVariant conversion stay here;
// callers consume only the typed application preference.
class ApplicationServicesClassVisibilityPreferencesPort final
    : public Application::ClassVisibilityPreferencesPort
{
public:
    explicit ApplicationServicesClassVisibilityPreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesClassVisibilityPreferencesPort(
        const ApplicationServicesClassVisibilityPreferencesPort&
        ) = delete;
    ApplicationServicesClassVisibilityPreferencesPort& operator=(
        const ApplicationServicesClassVisibilityPreferencesPort&
        ) = delete;
    ApplicationServicesClassVisibilityPreferencesPort(
        ApplicationServicesClassVisibilityPreferencesPort&&
        ) = delete;
    ApplicationServicesClassVisibilityPreferencesPort& operator=(
        ApplicationServicesClassVisibilityPreferencesPort&&
        ) = delete;

    [[nodiscard]] Application::ClassVisibilityScope load() const override
    {
        const SettingsService* settingsService = m_services.settingsService();
        if (!settingsService || !settingsService->isAvailable())
        {
            return Application::ClassVisibilityScope::ActiveSchedule;
        }

        const auto storedScope = settingsService->load(key());
        if (!storedScope || !storedScope->isValid())
        {
            // Preserve the legacy preference boundary's default materializing
            // behavior when storage is available.
            static_cast<void>(
                settingsService->save(
                    key(),
                    storedScopeValue(
                        Application::ClassVisibilityScope::ActiveSchedule
                        )
                    )
                );
            return Application::ClassVisibilityScope::ActiveSchedule;
        }

        return storedScope->toString().trimmed().toLower()
            == QStringLiteral("all_classes")
            ? Application::ClassVisibilityScope::AllClasses
            : Application::ClassVisibilityScope::ActiveSchedule;
    }

    void save(const Application::ClassVisibilityScope scope) const override
    {
        const SettingsService* settingsService = m_services.settingsService();
        if (!settingsService || !settingsService->isAvailable())
        {
            return;
        }

        static_cast<void>(
            settingsService->save(key(), storedScopeValue(scope))
            );
    }

private:
    [[nodiscard]] static QString key()
    {
        return QStringLiteral("classes_navigation_visibility_scope");
    }

    [[nodiscard]] static QString storedScopeValue(
        const Application::ClassVisibilityScope scope
        )
    {
        return scope == Application::ClassVisibilityScope::AllClasses
            ? QStringLiteral("all_classes")
            : QStringLiteral("active_schedule");
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
