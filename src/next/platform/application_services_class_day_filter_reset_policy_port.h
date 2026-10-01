#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/class_day_filter_reset_policy.h"

#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the class day-filter reset policy. The active-session
// settings access and QVariant conversion stay here; callers consume only the
// typed application policy.
class ApplicationServicesClassDayFilterResetPolicyPort final
    : public Application::ClassDayFilterResetPolicyPort
{
public:
    explicit ApplicationServicesClassDayFilterResetPolicyPort(
        ApplicationServices& services
        ) noexcept
        : m_session(services.databaseSession())
    {
    }

    ApplicationServicesClassDayFilterResetPolicyPort(
        const ApplicationServicesClassDayFilterResetPolicyPort&
        ) = delete;
    ApplicationServicesClassDayFilterResetPolicyPort& operator=(
        const ApplicationServicesClassDayFilterResetPolicyPort&
        ) = delete;
    ApplicationServicesClassDayFilterResetPolicyPort(
        ApplicationServicesClassDayFilterResetPolicyPort&&
        ) = delete;
    ApplicationServicesClassDayFilterResetPolicyPort& operator=(
        ApplicationServicesClassDayFilterResetPolicyPort&&
        ) = delete;

    [[nodiscard]] Application::ClassDayFilterResetPolicy load()
        const override
    {
        if (!m_session || !m_session->isOpen())
        {
            return Application::ClassDayFilterResetPolicy::OnApplicationClose;
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return Application::ClassDayFilterResetPolicy::OnApplicationClose;
        }

        const auto storedPolicy = repository->loadSetting(key());
        if (!storedPolicy || !storedPolicy->isValid())
        {
            // Preserve the legacy preference boundary's default materializing
            // behavior when storage is available.
            static_cast<void>(
                repository->saveSetting(
                    key(),
                    storedPolicyValue(
                        Application::ClassDayFilterResetPolicy::OnApplicationClose
                        )
                    )
                );
            return Application::ClassDayFilterResetPolicy::OnApplicationClose;
        }

        return storedPolicy->toString().trimmed().toLower()
            == QStringLiteral("on_page_leave")
            ? Application::ClassDayFilterResetPolicy::OnPageLeave
            : Application::ClassDayFilterResetPolicy::OnApplicationClose;
    }

    void save(
        const Application::ClassDayFilterResetPolicy policy
        ) const override
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
            repository->saveSetting(key(), storedPolicyValue(policy))
            );
    }

private:
    [[nodiscard]] static QString key()
    {
        return QStringLiteral(
            "classes_navigation_day_filter_reset_policy"
            );
    }

    [[nodiscard]] static QString storedPolicyValue(
        const Application::ClassDayFilterResetPolicy policy
        )
    {
        return policy == Application::ClassDayFilterResetPolicy::OnPageLeave
            ? QStringLiteral("on_page_leave")
            : QStringLiteral("on_application_close");
    }

    DatabaseSession* m_session = nullptr;
};

} // namespace ClassMngr::Next::Platform
