#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/evaluation_default_policy_preferences.h"

#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the evaluation-default policy. The legacy
// ApplicationServices/settings access and stored-string conversion are kept
// here; callers consume only the typed application policy.
class ApplicationServicesEvaluationDefaultPolicyPort final
    : public Application::EvaluationDefaultPolicyPreferencesPort
{
public:
    explicit ApplicationServicesEvaluationDefaultPolicyPort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesEvaluationDefaultPolicyPort(
        const ApplicationServicesEvaluationDefaultPolicyPort&
        ) = delete;
    ApplicationServicesEvaluationDefaultPolicyPort& operator=(
        const ApplicationServicesEvaluationDefaultPolicyPort&
        ) = delete;
    ApplicationServicesEvaluationDefaultPolicyPort(
        ApplicationServicesEvaluationDefaultPolicyPort&&
        ) = delete;
    ApplicationServicesEvaluationDefaultPolicyPort& operator=(
        ApplicationServicesEvaluationDefaultPolicyPort&&
        ) = delete;

    [[nodiscard]] Application::EvaluationDefaultPolicy load()
        const override
    {
        const SettingsService* settingsService = m_services.settingsService();
        if (!settingsService || !settingsService->isAvailable())
        {
            return Application::EvaluationDefaultPolicy::All;
        }

        const auto storedPolicy = settingsService->load(key());
        if (!storedPolicy || !storedPolicy->isValid())
        {
            // Preserve the legacy preference boundary's default materializing
            // behavior when storage is available.
            static_cast<void>(
                settingsService->save(
                    key(),
                    storedPolicyValue(Application::EvaluationDefaultPolicy::All)
                    )
                );
            return Application::EvaluationDefaultPolicy::All;
        }

        return storedPolicy->toString().trimmed().toLower()
            == QStringLiteral("current_or_previous_term")
            ? Application::EvaluationDefaultPolicy::CurrentOrPreviousTerm
            : Application::EvaluationDefaultPolicy::All;
    }

    void save(
        const Application::EvaluationDefaultPolicy policy
        ) const override
    {
        const SettingsService* settingsService = m_services.settingsService();
        if (!settingsService || !settingsService->isAvailable())
        {
            return;
        }

        static_cast<void>(settingsService->save(key(), storedPolicyValue(policy)));
    }

private:
    [[nodiscard]] static QString key()
    {
        return QStringLiteral(
            "classes_navigation_evaluation_default_policy"
            );
    }

    [[nodiscard]] static QString storedPolicyValue(
        const Application::EvaluationDefaultPolicy policy
        )
    {
        return policy
            == Application::EvaluationDefaultPolicy::CurrentOrPreviousTerm
            ? QStringLiteral("current_or_previous_term")
            : QStringLiteral("all");
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
