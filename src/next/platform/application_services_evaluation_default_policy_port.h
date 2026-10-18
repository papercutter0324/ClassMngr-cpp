#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/evaluation_default_policy_preferences.h"

#include <QString>
#include <QVariant>

#include <string>
#include <utility>

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

    [[nodiscard]] Application::EvaluationDefaultPolicyPreferencesLoadResult
    load() const override
    {
        if (!m_session || !m_session->isOpen())
        {
            return Application::
                EvaluationDefaultPolicyPreferencesLoadResult::success(
                    Application::EvaluationDefaultPolicy::All
                    );
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return Application::
                EvaluationDefaultPolicyPreferencesLoadResult::success(
                    Application::EvaluationDefaultPolicy::All
                    );
        }

        const auto storedPolicy = repository->loadSetting(key());
        if (!storedPolicy)
        {
            const QByteArray errorBytes = storedPolicy.error().toUtf8();
            return Application::
                EvaluationDefaultPolicyPreferencesLoadResult::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = errorBytes.isEmpty()
                        ? "Evaluation default policy could not be loaded."
                        : errorBytes.toStdString(),
                    .recoverable = false
                });
        }

        if (!storedPolicy->isValid())
        {
            // Preserve the legacy preference boundary's default materializing
            // behavior for a successfully read, missing value.
            static_cast<void>(
                repository->saveSetting(
                    key(),
                    storedPolicyValue(Application::EvaluationDefaultPolicy::All)
                    )
                );
            return Application::
                EvaluationDefaultPolicyPreferencesLoadResult::success(
                    Application::EvaluationDefaultPolicy::All
                    );
        }

        return Application::
            EvaluationDefaultPolicyPreferencesLoadResult::success(
                storedPolicy->toString().trimmed().toLower()
                    == QStringLiteral("current_or_previous_term")
                    ? Application::EvaluationDefaultPolicy::CurrentOrPreviousTerm
                    : Application::EvaluationDefaultPolicy::All
                );
    }

    [[nodiscard]] Application::EvaluationDefaultPolicyPreferencesSaveResult
    save(
        const Application::EvaluationDefaultPolicy policy
        ) const override
    {
        if (policy != Application::EvaluationDefaultPolicy::All
            && policy
                != Application::EvaluationDefaultPolicy::CurrentOrPreviousTerm)
        {
            return Application::
                EvaluationDefaultPolicyPreferencesSaveResult::failure({
                    .code = Domain::ErrorCode::InvalidInput,
                    .message = "Evaluation default policy is invalid.",
                    .recoverable = false
                });
        }

        if (!m_session || !m_session->isOpen())
        {
            return failure(
                "Evaluation default policy cannot be saved because no database session is open."
                );
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return failure(
                "Evaluation default policy cannot be saved because the settings repository is unavailable."
                );
        }

        const Status saved = repository->saveSetting(
            key(),
            storedPolicyValue(policy)
            );
        if (!saved)
        {
            const QByteArray errorBytes = saved.error().toUtf8();
            return failure(
                errorBytes.isEmpty()
                    ? "Evaluation default policy could not be saved."
                    : errorBytes.toStdString()
                );
        }

        return Application::
            EvaluationDefaultPolicyPreferencesSaveResult::success();
    }

private:
    [[nodiscard]] static Application::
        EvaluationDefaultPolicyPreferencesSaveResult failure(
            std::string message
            )
    {
        return Application::EvaluationDefaultPolicyPreferencesSaveResult::
            failure({
                .code = Domain::ErrorCode::Technical,
                .message = std::move(message),
                .recoverable = false
            });
    }

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
