#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/evaluation_default_policy_preferences.h"

#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the evaluation-default policy. The active-session
// settings access and stored-string conversion are kept here; callers consume
// only the typed application policy.
class ApplicationServicesEvaluationDefaultPolicyPort final
    : public Application::EvaluationDefaultPolicyPreferencesPort
{
public:
    explicit ApplicationServicesEvaluationDefaultPolicyPort(
        ApplicationServices& services
        ) noexcept
        : m_session(services.databaseSession())
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
        if (!m_session || !m_session->isOpen())
        {
            return Application::EvaluationDefaultPolicy::All;
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return Application::EvaluationDefaultPolicy::All;
        }

        const auto storedPolicy = repository->loadSetting(key());
        if (!storedPolicy || !storedPolicy->isValid())
        {
            // Preserve the legacy preference boundary's default materializing
            // behavior when storage is available.
            static_cast<void>(
                repository->saveSetting(
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
        if (!m_session || !m_session->isOpen())
        {
            return;
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return;
        }

        static_cast<void>(repository->saveSetting(key(), storedPolicyValue(policy)));
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

    DatabaseSession* m_session = nullptr;
};

} // namespace ClassMngr::Next::Platform
