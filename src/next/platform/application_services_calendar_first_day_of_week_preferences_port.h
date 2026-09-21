#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/calendar_first_day_of_week_preferences.h"

#include <QDebug>
#include <QLocale>
#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the calendar first-day preference. The existing
// SettingsService lifetime and QVariant/QLocale conversion remain outside the
// application contract; the provider retains its UI normalization policy.
class ApplicationServicesCalendarFirstDayOfWeekPreferencesPort final
    : public Application::CalendarFirstDayOfWeekPreferencesPort
{
public:
    explicit ApplicationServicesCalendarFirstDayOfWeekPreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_settingsService(services.settingsService())
    {
    }

    explicit ApplicationServicesCalendarFirstDayOfWeekPreferencesPort(
        SettingsService* settingsService
        ) noexcept
        : m_settingsService(settingsService)
    {
    }

    ApplicationServicesCalendarFirstDayOfWeekPreferencesPort(
        const ApplicationServicesCalendarFirstDayOfWeekPreferencesPort&
        ) = delete;
    ApplicationServicesCalendarFirstDayOfWeekPreferencesPort& operator=(
        const ApplicationServicesCalendarFirstDayOfWeekPreferencesPort&
        ) = delete;
    ApplicationServicesCalendarFirstDayOfWeekPreferencesPort(
        ApplicationServicesCalendarFirstDayOfWeekPreferencesPort&&
        ) = delete;
    ApplicationServicesCalendarFirstDayOfWeekPreferencesPort& operator=(
        ApplicationServicesCalendarFirstDayOfWeekPreferencesPort&&
        ) = delete;

    [[nodiscard]] Application::CalendarFirstDayOfWeek load()
        const override
    {
        const Application::CalendarFirstDayOfWeek fallback =
            localeFirstDayOfWeek();

        if (!m_settingsService || !m_settingsService->isAvailable())
        {
            return fallback;
        }

        const QVariant storedValue = m_settingsService->loadOrDefault(
            key(),
            static_cast<int>(fallback)
            );

        bool ok = false;
        const int storedDay = storedValue.toInt(&ok);
        if (!ok || storedDay < 0 || storedDay > 6)
        {
            return fallback;
        }

        return static_cast<Application::CalendarFirstDayOfWeek>(storedDay);
    }

    void save(
        const Application::CalendarFirstDayOfWeek firstDayOfWeek
        ) const override
    {
        if (!m_settingsService || !m_settingsService->isAvailable())
        {
            return;
        }

        if (const Status saved = m_settingsService->save(
                key(),
                static_cast<int>(firstDayOfWeek)
                ); !saved)
        {
            qWarning()
                << "Failed to save calendar first-day preference:"
                << saved.error();
        }
    }

private:
    [[nodiscard]] static QString key()
    {
        return QStringLiteral("calendar/firstDayOfWeek");
    }

    [[nodiscard]] static Application::CalendarFirstDayOfWeek
    localeFirstDayOfWeek()
    {
        switch (QLocale().firstDayOfWeek())
        {
        case Qt::Sunday:
            return Application::CalendarFirstDayOfWeek::Sunday;
        case Qt::Monday:
            return Application::CalendarFirstDayOfWeek::Monday;
        case Qt::Tuesday:
            return Application::CalendarFirstDayOfWeek::Tuesday;
        case Qt::Wednesday:
            return Application::CalendarFirstDayOfWeek::Wednesday;
        case Qt::Thursday:
            return Application::CalendarFirstDayOfWeek::Thursday;
        case Qt::Friday:
            return Application::CalendarFirstDayOfWeek::Friday;
        case Qt::Saturday:
            return Application::CalendarFirstDayOfWeek::Saturday;
        }

        return Application::CalendarFirstDayOfWeek::Sunday;
    }

    SettingsService* m_settingsService = nullptr;
};

} // namespace ClassMngr::Next::Platform
