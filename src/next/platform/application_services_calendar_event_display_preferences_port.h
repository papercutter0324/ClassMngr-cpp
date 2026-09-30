#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/calendar_event_display_preferences.h"

#include <QByteArray>
#include <QString>
#include <QVariant>

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
        : m_session(services.databaseSession())
    {
    }

    explicit ApplicationServicesCalendarEventDisplayPreferencesPort(
        ApplicationServices* services
        ) noexcept
        : m_session(services ? services->databaseSession() : nullptr)
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
        if (!m_session || !m_session->isOpen())
        {
            return Application::CalendarEventDisplayPreferencesResult::success({});
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return Application::CalendarEventDisplayPreferencesResult::success({});
        }

        const auto showAllCampuses = repository->loadSetting(
            showEventsAtAllCampusesKey()
            );
        const auto hideStartOfTerm = repository->loadSetting(
            hideStartOfTermEventsKey()
            );
        return Application::CalendarEventDisplayPreferencesResult::success({
            .showEventsAtAllCampuses = settingToBool(
                showAllCampuses ? *showAllCampuses : QVariant(),
                false
                ),
            .hideStartOfTermEvents = settingToBool(
                hideStartOfTerm ? *hideStartOfTerm : QVariant(),
                false
                )
        });
    }

    [[nodiscard]] Application::CalendarEventDisplayPreferencesSaveResult
    save(
        const Application::CalendarEventDisplayPreferencesSaveRequest& request
        ) const override
    {
        if (!m_session || !m_session->isOpen())
        {
            return Application::
                CalendarEventDisplayPreferencesSaveResult::success();
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return Application::
                CalendarEventDisplayPreferencesSaveResult::success();
        }

        const Status saved = repository->saveSettings({
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
            const QByteArray errorBytes = saved.error().toUtf8();
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

    DatabaseSession* m_session = nullptr;
};

} // namespace ClassMngr::Next::Platform
