#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/class_visibility_preferences.h"

#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the class visibility scope. The active-session
// settings access and QVariant conversion stay here; callers consume only the
// typed application preference.
class ApplicationServicesClassVisibilityPreferencesPort final
    : public Application::ClassVisibilityPreferencesPort
{
public:
    explicit ApplicationServicesClassVisibilityPreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_session(services.databaseSession())
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
        if (!m_session || !m_session->isOpen())
        {
            return Application::ClassVisibilityScope::ActiveSchedule;
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return Application::ClassVisibilityScope::ActiveSchedule;
        }

        const auto storedScope = repository->loadSetting(key());
        if (!storedScope || !storedScope->isValid())
        {
            // Preserve the legacy preference boundary's default materializing
            // behavior when storage is available.
            static_cast<void>(
                repository->saveSetting(
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
        if (!m_session || !m_session->isOpen())
        {
            return;
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return;
        }

        static_cast<void>(
            repository->saveSetting(key(), storedScopeValue(scope))
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

    DatabaseSession* m_session = nullptr;
};

} // namespace ClassMngr::Next::Platform
