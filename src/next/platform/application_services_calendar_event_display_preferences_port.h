#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/settings_repository.h"
#include "next/application/calendar_event_display_preferences.h"

#include <QString>
#include <QVariant>

#include <string>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the two calendar event-display settings. The
// ApplicationServices/settings access and QVariant conversion stay here;
// saveSettings keeps the pair atomic and leaves unrelated settings untouched.
class ApplicationServicesCalendarEventDisplayPreferencesPort final
    : public Application::CalendarEventDisplayPreferencesPort
{
public:
    explicit ApplicationServicesCalendarEventDisplayPreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_store(services.workspaceStore())
    {
    }

    explicit ApplicationServicesCalendarEventDisplayPreferencesPort(
        ApplicationServices* services
        ) noexcept
        : m_store(services ? services->workspaceStore() : nullptr)
    {
    }

    ApplicationServicesCalendarEventDisplayPreferencesPort(
        const ApplicationServicesCalendarEventDisplayPreferencesPort&
        ) = delete;
    ApplicationServicesCalendarEventDisplayPreferencesPort& operator=(
        const ApplicationServicesCalendarEventDisplayPreferencesPort&
        ) = delete;
    ApplicationServicesCalendarEventDisplayPreferencesPort(
        ApplicationServicesCalendarEventDisplayPreferencesPort&&
        ) = delete;
    ApplicationServicesCalendarEventDisplayPreferencesPort& operator=(
        ApplicationServicesCalendarEventDisplayPreferencesPort&&
        ) = delete;

    [[nodiscard]] Application::CalendarEventDisplayPreferencesResult load()
        const override
    {
        if (!m_store || !m_store->isOpen())
        {
            return Application::CalendarEventDisplayPreferencesResult::success({});
        }

        SettingsRepository* const repository = m_store->settingsRepository();
        if (!repository)
        {
            return Application::CalendarEventDisplayPreferencesResult::success({});
        }

        const auto showAllCampuses = repository->loadSetting(
            showEventsAtAllCampusesKey()
            );
        if (!showAllCampuses)
        {
            return Application::CalendarEventDisplayPreferencesResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = settingLoadFailure(
                    showEventsAtAllCampusesKey(),
                    toLegacyDiagnostic(showAllCampuses.error())
                    ),
                .recoverable = false
            });
        }

        const auto hideStartOfTerm = repository->loadSetting(
            hideStartOfTermEventsKey()
            );
        if (!hideStartOfTerm)
        {
            return Application::CalendarEventDisplayPreferencesResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = settingLoadFailure(
                    hideStartOfTermEventsKey(),
                    toLegacyDiagnostic(hideStartOfTerm.error())
                    ),
                .recoverable = false
            });
        }

        return Application::CalendarEventDisplayPreferencesResult::success({
            .showEventsAtAllCampuses = settingToBool(
                showAllCampuses.value(),
                false
                ),
            .hideStartOfTermEvents = settingToBool(
                hideStartOfTerm.value(),
                false
                )
        });
    }

    [[nodiscard]] Application::CalendarEventDisplayPreferencesSaveResult
    save(
        const Application::CalendarEventDisplayPreferencesSaveRequest& request
        ) const override
    {
        if (!m_store || !m_store->isOpen())
        {
            return Application::
                CalendarEventDisplayPreferencesSaveResult::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message =
                        "Calendar event-display preferences cannot be saved because no database session is open.",
                    .recoverable = false
                });
        }

        SettingsRepository* const repository = m_store->settingsRepository();
        if (!repository)
        {
            return Application::
                CalendarEventDisplayPreferencesSaveResult::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message =
                        "Calendar event-display preferences cannot be saved because the settings repository is unavailable.",
                    .recoverable = false
                });
        }

        const RepositoryStatus saved = repository->saveSettings({
            {
                showEventsAtAllCampusesKey(),
                request.showEventsAtAllCampuses
            },
            {
                hideStartOfTermEventsKey(),
                request.hideStartOfTermEvents
            }
        });
        if (!saved)
        {
            const QByteArray errorBytes = toLegacyDiagnostic(saved.error()).toUtf8();
            return Application::
                CalendarEventDisplayPreferencesSaveResult::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = errorBytes.isEmpty()
                        ? "Calendar event-display preferences could not be saved."
                        : errorBytes.toStdString(),
                    .recoverable = false
                });
        }

        return Application::
            CalendarEventDisplayPreferencesSaveResult::success();
    }

private:
    [[nodiscard]] static std::string settingLoadFailure(
        const QString& key,
        const QString& diagnostic
        )
    {
        std::string message = "Calendar event-display preference '";
        message.append(key.toStdString());
        message.append("' could not be loaded");
        if (!diagnostic.isEmpty())
        {
            message.append(": ");
            message.append(diagnostic.toStdString());
        }
        message.push_back('.');
        return message;
    }

    [[nodiscard]] static QString showEventsAtAllCampusesKey()
    {
        return QStringLiteral("calendar/showEventsAtAllCampuses");
    }

    [[nodiscard]] static QString hideStartOfTermEventsKey()
    {
        return QStringLiteral("calendar/hideStartOfTermEvents");
    }

    [[nodiscard]] static bool settingToBool(
        const QVariant& value,
        const bool defaultValue
        )
    {
        if (!value.isValid())
        {
            return defaultValue;
        }

        const QString text = value.toString().trimmed().toLower();
        if (text == QStringLiteral("true") || text == QStringLiteral("1"))
        {
            return true;
        }

        if (text == QStringLiteral("false") || text == QStringLiteral("0"))
        {
            return false;
        }

        return value.toBool();
    }

    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform
