#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/settings_repository.h"
#include "next/application/class_day_filter_reset_policy.h"

#include <QByteArray>
#include <QString>
#include <QVariant>

#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the class day-filter reset policy. Workspace settings
// access and QVariant conversion stay here; callers consume only the
// typed application policy.
class ApplicationServicesClassDayFilterResetPolicyPort final
    : public Application::ClassDayFilterResetPolicyPort
{
public:
    explicit ApplicationServicesClassDayFilterResetPolicyPort(
        ApplicationServices& services
        ) noexcept
        : m_store(services.workspaceStore())
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

    [[nodiscard]] Application::ClassDayFilterResetPolicyLoadResult load()
        const override
    {
        if (!m_store || !m_store->isOpen())
        {
            return Application::ClassDayFilterResetPolicyLoadResult::success(
                Application::ClassDayFilterResetPolicy::OnApplicationClose
                );
        }

        SettingsRepository* const repository = m_store->settingsRepository();
        if (!repository)
        {
            return Application::ClassDayFilterResetPolicyLoadResult::success(
                Application::ClassDayFilterResetPolicy::OnApplicationClose
                );
        }

        const auto storedPolicy = repository->loadSetting(key());
        if (!storedPolicy)
        {
            const QByteArray errorBytes = toLegacyDiagnostic(storedPolicy.error()).toUtf8();
            return Application::ClassDayFilterResetPolicyLoadResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = errorBytes.isEmpty()
                    ? "Class day-filter reset policy could not be loaded."
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
                        Application::ClassDayFilterResetPolicy::OnApplicationClose
                        )
                    )
                );
            return Application::ClassDayFilterResetPolicyLoadResult::success(
                Application::ClassDayFilterResetPolicy::OnApplicationClose
                );
        }

        return Application::ClassDayFilterResetPolicyLoadResult::success(
            storedPolicy->toString().trimmed().toLower()
                    == QStringLiteral("on_page_leave")
                ? Application::ClassDayFilterResetPolicy::OnPageLeave
                : Application::ClassDayFilterResetPolicy::OnApplicationClose
            );
    }

    [[nodiscard]] Application::ClassDayFilterResetPolicySaveResult save(
        const Application::ClassDayFilterResetPolicy policy
        ) const override
    {
        if (policy != Application::ClassDayFilterResetPolicy::OnApplicationClose
            && policy != Application::ClassDayFilterResetPolicy::OnPageLeave)
        {
            return Application::ClassDayFilterResetPolicySaveResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Class day-filter reset policy is invalid.",
                .recoverable = false
            });
        }

        if (!m_store || !m_store->isOpen())
        {
            return failure(
                "Class day-filter reset policy cannot be saved because no database session is open."
                );
        }

        SettingsRepository* const repository = m_store->settingsRepository();
        if (!repository)
        {
            return failure(
                "Class day-filter reset policy cannot be saved because the settings repository is unavailable."
                );
        }

        const RepositoryStatus saved = repository->saveSetting(
            key(),
            storedPolicyValue(policy)
            );
        if (!saved)
        {
            const QByteArray errorBytes = toLegacyDiagnostic(saved.error()).toUtf8();
            return failure(
                errorBytes.isEmpty()
                    ? "Class day-filter reset policy could not be saved."
                    : errorBytes.toStdString()
                );
        }

        return Application::ClassDayFilterResetPolicySaveResult::success();
    }

private:
    [[nodiscard]] static Application::ClassDayFilterResetPolicySaveResult
    failure(std::string message)
    {
        return Application::ClassDayFilterResetPolicySaveResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = std::move(message),
            .recoverable = false
        });
    }

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

    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform
