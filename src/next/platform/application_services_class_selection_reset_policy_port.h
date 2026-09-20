#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/class_selection_reset_policy.h"

#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the class-selection reset policy. The legacy
// ApplicationServices/settings access and QVariant conversion stay here;
// callers consume only the typed application policy.
class ApplicationServicesClassSelectionResetPolicyPort final
    : public Application::ClassSelectionResetPolicyPort
{
public:
    explicit ApplicationServicesClassSelectionResetPolicyPort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesClassSelectionResetPolicyPort(
        const ApplicationServicesClassSelectionResetPolicyPort&
        ) = delete;
    ApplicationServicesClassSelectionResetPolicyPort& operator=(
        const ApplicationServicesClassSelectionResetPolicyPort&
        ) = delete;
    ApplicationServicesClassSelectionResetPolicyPort(
        ApplicationServicesClassSelectionResetPolicyPort&&
        ) = delete;
    ApplicationServicesClassSelectionResetPolicyPort& operator=(
        ApplicationServicesClassSelectionResetPolicyPort&&
        ) = delete;

    [[nodiscard]] Application::ClassSelectionResetPolicy load()
        const override
    {
        const SettingsService* settingsService = m_services.settingsService();
        if (!settingsService || !settingsService->isAvailable())
        {
            return Application::ClassSelectionResetPolicy::OnApplicationClose;
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
                        Application::ClassSelectionResetPolicy::OnApplicationClose
                        )
                    )
                );
            return Application::ClassSelectionResetPolicy::OnApplicationClose;
        }

        return storedPolicy->toString().trimmed().toLower()
            == QStringLiteral("on_page_leave")
            ? Application::ClassSelectionResetPolicy::OnPageLeave
            : Application::ClassSelectionResetPolicy::OnApplicationClose;
    }

    void save(
        const Application::ClassSelectionResetPolicy policy
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
            "classes_navigation_class_selection_reset_policy"
            );
    }

    [[nodiscard]] static QString storedPolicyValue(
        const Application::ClassSelectionResetPolicy policy
        )
    {
        return policy == Application::ClassSelectionResetPolicy::OnPageLeave
            ? QStringLiteral("on_page_leave")
            : QStringLiteral("on_application_close");
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
