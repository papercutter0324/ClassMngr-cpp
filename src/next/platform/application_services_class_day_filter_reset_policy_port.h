#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/class_day_filter_reset_policy.h"

#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the class day-filter reset policy. The legacy
// ApplicationServices/settings access and QVariant conversion stay here;
// callers consume only the typed application policy.
class ApplicationServicesClassDayFilterResetPolicyPort final
    : public Application::ClassDayFilterResetPolicyPort
{
public:
    explicit ApplicationServicesClassDayFilterResetPolicyPort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
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
        const SettingsService* settingsService = m_services.settingsService();
        if (!settingsService || !settingsService->isAvailable())
        {
            return Application::ClassDayFilterResetPolicy::OnApplicationClose;
        }

        const auto storedPolicy = settingsService->load(key());
        if (!storedPolicy || !storedPolicy->isValid())
        {
            // Preserve the legacy preference boundary's default materializing
            // behavior when storage is available.
            static_cast<void>(
                settingsService->save(
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
        const SettingsService* settingsService = m_services.settingsService();
        if (!settingsService || !settingsService->isAvailable())
        {
            return;
        }

        static_cast<void>(
            settingsService->save(key(), storedPolicyValue(policy))
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

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
