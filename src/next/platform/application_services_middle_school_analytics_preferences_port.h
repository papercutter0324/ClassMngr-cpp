#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/middle_school_analytics_preferences.h"

#include <QString>
#include <QVariant>

#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the middle-school analytics visibility preference.
// The active-session settings access and QVariant conversion stay here;
// callers consume only the typed application boolean.
class ApplicationServicesMiddleSchoolAnalyticsPreferencesPort final
    : public Application::MiddleSchoolAnalyticsPreferencesPort
{
public:
    explicit ApplicationServicesMiddleSchoolAnalyticsPreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_session(services.databaseSession())
    {
    }

    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort(
        const ApplicationServicesMiddleSchoolAnalyticsPreferencesPort&
        ) = delete;
    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort& operator=(
        const ApplicationServicesMiddleSchoolAnalyticsPreferencesPort&
        ) = delete;
    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort(
        ApplicationServicesMiddleSchoolAnalyticsPreferencesPort&&
        ) = delete;
    ApplicationServicesMiddleSchoolAnalyticsPreferencesPort& operator=(
        ApplicationServicesMiddleSchoolAnalyticsPreferencesPort&&
        ) = delete;

    [[nodiscard]] Application::MiddleSchoolAnalyticsPreferencesReadResult
    load() const override
    {
        if (!m_session || !m_session->isOpen())
        {
            return Application::MiddleSchoolAnalyticsPreferencesReadResult::
                success(false);
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return Application::MiddleSchoolAnalyticsPreferencesReadResult::
                success(false);
        }

        const auto storedValue = repository->loadSetting(key());
        if (!storedValue)
        {
            const QByteArray errorBytes = storedValue.error().toUtf8();
            return Application::MiddleSchoolAnalyticsPreferencesReadResult::
                failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = errorBytes.isEmpty()
                        ? "Middle-school analytics preference could not be loaded."
                        : errorBytes.toStdString(),
                    .recoverable = false
                });
        }

        if (!storedValue->isValid())
        {
            // Preserve the legacy preference boundary's default materializing
            // behavior when storage is available.
            static_cast<void>(repository->saveSetting(key(), false));
            return Application::MiddleSchoolAnalyticsPreferencesReadResult::
                success(false);
        }

        // Preserve the previous QVariant coercion for stored values.
        return Application::MiddleSchoolAnalyticsPreferencesReadResult::
            success(storedValue->toBool());
    }

    [[nodiscard]] Application::MiddleSchoolAnalyticsPreferencesSaveResult save(
        const bool show
        ) const override
    {
        if (!m_session || !m_session->isOpen())
        {
            return failure(
                "Middle-school analytics preference cannot be saved because no database session is open."
                );
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return failure(
                "Middle-school analytics preference cannot be saved because the settings repository is unavailable."
                );
        }

        const Status saved = repository->saveSetting(key(), show);
        if (!saved)
        {
            const QByteArray errorBytes = saved.error().toUtf8();
            return failure(
                errorBytes.isEmpty()
                    ? "Middle-school analytics preference could not be saved."
                    : errorBytes.toStdString()
                );
        }

        return Application::MiddleSchoolAnalyticsPreferencesSaveResult::success();
    }

private:
    [[nodiscard]] static Application::MiddleSchoolAnalyticsPreferencesSaveResult
    failure(std::string message)
    {
        return Application::MiddleSchoolAnalyticsPreferencesSaveResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = std::move(message),
                .recoverable = false
            });
    }

    [[nodiscard]] static QString key()
    {
        return QStringLiteral(
            "classes_navigation_show_middle_school_analytics_and_evaluations"
            );
    }

    DatabaseSession* m_session = nullptr;
};

} // namespace ClassMngr::Next::Platform
