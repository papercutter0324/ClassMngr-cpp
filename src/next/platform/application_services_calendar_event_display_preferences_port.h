#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/calendar_event_display_preferences.h"

#include <QByteArray>
#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the two calendar event-display settings. The
// ApplicationServices/settings access and QVariant conversion stay here;
// saveAll keeps the pair atomic and leaves unrelated settings untouched.
class ApplicationServicesCalendarEventDisplayPreferencesPort final
    : public Application::CalendarEventDisplayPreferencesPort
{
public:
    explicit ApplicationServicesCalendarEventDisplayPreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_settingsService(services.settingsService())
    {
    }

    explicit ApplicationServicesCalendarEventDisplayPreferencesPort(
        ApplicationServices* services
        ) noexcept
        : m_settingsService(
            services ? services->settingsService() : nullptr
            )
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
        if (!m_settingsService || !m_settingsService->isAvailable())
        {
            return Application::CalendarEventDisplayPreferencesResult::success({});
        }

        return Application::CalendarEventDisplayPreferencesResult::success({
            .showEventsAtAllCampuses = settingToBool(
                m_settingsService->loadOrDefault(
                    showEventsAtAllCampusesKey(),
                    false
                    ),
                false
                ),
            .hideStartOfTermEvents = settingToBool(
                m_settingsService->loadOrDefault(
                    hideStartOfTermEventsKey(),
                    false
                    ),
                false
                )
        });
    }

    [[nodiscard]] Application::CalendarEventDisplayPreferencesSaveResult
    save(
        const Application::CalendarEventDisplayPreferencesSaveRequest& request
        ) const override
    {
        if (!m_settingsService || !m_settingsService->isAvailable())
        {
            return Application::
                CalendarEventDisplayPreferencesSaveResult::success();
        }

        const Status saved = m_settingsService->saveAll({
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

    SettingsService* m_settingsService = nullptr;
};

} // namespace ClassMngr::Next::Platform
