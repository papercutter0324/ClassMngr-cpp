#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/class_selection_reset_policy.h"

#include <QByteArray>
#include <QString>
#include <QVariant>

#include <string>
#include <utility>

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

    [[nodiscard]] Application::ClassSelectionResetPolicyLoadResult load()
        const override
    {
        if (!m_session || !m_session->isOpen())
        {
            return Application::ClassSelectionResetPolicyLoadResult::success(
                Application::ClassSelectionResetPolicy::OnApplicationClose
                );
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return Application::ClassSelectionResetPolicyLoadResult::success(
                Application::ClassSelectionResetPolicy::OnApplicationClose
                );
        }

        const auto storedPolicy = repository->loadSetting(key());
        if (!storedPolicy)
        {
            const QByteArray errorBytes = storedPolicy.error().toUtf8();
            return Application::ClassSelectionResetPolicyLoadResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = errorBytes.isEmpty()
                    ? "Class selection reset policy could not be loaded."
                    : errorBytes.toStdString(),
                .recoverable = true
            });
        }

        if (!storedPolicy->isValid())
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
            return Application::ClassSelectionResetPolicyLoadResult::success(
                Application::ClassSelectionResetPolicy::OnApplicationClose
                );
        }

        return Application::ClassSelectionResetPolicyLoadResult::success(
            storedPolicy->toString().trimmed().toLower()
                    == QStringLiteral("on_page_leave")
                ? Application::ClassSelectionResetPolicy::OnPageLeave
                : Application::ClassSelectionResetPolicy::OnApplicationClose
            );
    }

    [[nodiscard]] Application::ClassSelectionResetPolicySaveResult save(
        const Application::ClassSelectionResetPolicy policy
        ) const override
    {
        if (policy != Application::ClassSelectionResetPolicy::OnApplicationClose
            && policy != Application::ClassSelectionResetPolicy::OnPageLeave)
        {
            return Application::ClassSelectionResetPolicySaveResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Class selection reset policy is invalid.",
                .recoverable = false
            });
        }

        if (!m_session || !m_session->isOpen())
        {
            return failure(
                "Class selection reset policy cannot be saved because no database session is open."
                );
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return failure(
                "Class selection reset policy cannot be saved because the settings repository is unavailable."
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
                    ? "Class selection reset policy could not be saved."
                    : errorBytes.toStdString()
                );
        }

        return Application::ClassSelectionResetPolicySaveResult::success();
    }

private:
    [[nodiscard]] static Application::ClassSelectionResetPolicySaveResult
    failure(std::string message)
    {
        return Application::ClassSelectionResetPolicySaveResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = std::move(message),
            .recoverable = false
        });
    }

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
