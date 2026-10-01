#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/class_selection_reset_policy.h"

#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the class-selection reset policy. The active-session
// settings access and QVariant conversion stay here; callers consume only the
// typed application policy.
class ApplicationServicesClassSelectionResetPolicyPort final
    : public Application::ClassSelectionResetPolicyPort
{
public:
    explicit ApplicationServicesClassSelectionResetPolicyPort(
        ApplicationServices& services
        ) noexcept
        : m_session(services.databaseSession())
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
        if (!m_session || !m_session->isOpen())
        {
            return Application::ClassSelectionResetPolicy::OnApplicationClose;
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return Application::ClassSelectionResetPolicy::OnApplicationClose;
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

    DatabaseSession* m_session = nullptr;
};

} // namespace ClassMngr::Next::Platform
