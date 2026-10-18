#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/calendar_first_day_of_week_preferences.h"

#include <QByteArray>
#include <QLocale>
#include <QString>
#include <QVariant>

#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the calendar first-day preference. The existing
// SettingsRepository access and QVariant/QLocale conversion remain outside
// the application contract; the provider retains its UI normalization policy.
class ApplicationServicesCalendarFirstDayOfWeekPreferencesPort final
    : public Application::CalendarFirstDayOfWeekPreferencesPort
{
public:
    explicit ApplicationServicesCalendarFirstDayOfWeekPreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_session(services.databaseSession())
    {
    }

    explicit ApplicationServicesCalendarFirstDayOfWeekPreferencesPort(
        ApplicationServices* services
        ) noexcept
        : m_session(services ? services->databaseSession() : nullptr)
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

    [[nodiscard]] Application::CalendarFirstDayOfWeekPreferencesLoadResult
    load() const override
    {
        const Application::CalendarFirstDayOfWeek fallback =
            localeFirstDayOfWeek();

        if (!m_session || !m_session->isOpen())
        {
            return Application::
                CalendarFirstDayOfWeekPreferencesLoadResult::success(fallback);
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return Application::
                CalendarFirstDayOfWeekPreferencesLoadResult::success(fallback);
        }

        const auto stored = repository->loadSetting(key());
        if (!stored)
        {
            return Application::
                CalendarFirstDayOfWeekPreferencesLoadResult::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = loadFailure(stored.error()),
                    .recoverable = false
                });
        }

        bool ok = false;
        const int storedDay = stored->toInt(&ok);
        if (!ok || storedDay < 0 || storedDay > 6)
        {
            return Application::
                CalendarFirstDayOfWeekPreferencesLoadResult::success(fallback);
        }

        return Application::
            CalendarFirstDayOfWeekPreferencesLoadResult::success(
                static_cast<Application::CalendarFirstDayOfWeek>(storedDay)
                );
    }

    [[nodiscard]] Application::
        CalendarFirstDayOfWeekPreferencesPort::SaveResult save(
        const Application::CalendarFirstDayOfWeek firstDayOfWeek
        ) const override
    {
        switch (firstDayOfWeek)
        {
        case Application::CalendarFirstDayOfWeek::Sunday:
        case Application::CalendarFirstDayOfWeek::Monday:
        case Application::CalendarFirstDayOfWeek::Tuesday:
        case Application::CalendarFirstDayOfWeek::Wednesday:
        case Application::CalendarFirstDayOfWeek::Thursday:
        case Application::CalendarFirstDayOfWeek::Friday:
        case Application::CalendarFirstDayOfWeek::Saturday:
            break;

        default:
            return Application::
                CalendarFirstDayOfWeekPreferencesPort::SaveResult::failure({
                    .code = Domain::ErrorCode::InvalidInput,
                    .message =
                        "Calendar first-day preference must be a valid day of the week.",
                    .recoverable = false
                });
        }

        if (!m_session || !m_session->isOpen())
        {
            return failure(
                "Calendar first-day preference cannot be saved because no database session is open."
                );
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return failure(
                "Calendar first-day preference cannot be saved because the settings repository is unavailable."
                );
        }

        if (const Status saved = repository->saveSetting(
                key(),
                static_cast<int>(firstDayOfWeek)
                ); !saved)
        {
            const QByteArray errorBytes = saved.error().toUtf8();
            return failure(
                errorBytes.isEmpty()
                    ? "Calendar first-day preference could not be saved."
                    : errorBytes.toStdString()
                );
        }

        return Application::
            CalendarFirstDayOfWeekPreferencesPort::SaveResult::success();
    }

private:
    [[nodiscard]] static std::string loadFailure(const QString& diagnostic)
    {
        std::string message = "Calendar first-day preference '";
        message.append(key().toStdString());
        message.append("' could not be loaded");
        if (!diagnostic.isEmpty())
        {
            message.append(": ");
            message.append(diagnostic.toStdString());
        }
        message.push_back('.');
        return message;
    }

    [[nodiscard]] static Application::
        CalendarFirstDayOfWeekPreferencesPort::SaveResult failure(
            std::string message
            )
    {
        return Application::
            CalendarFirstDayOfWeekPreferencesPort::SaveResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = std::move(message),
                .recoverable = false
            });
    }

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

    DatabaseSession* m_session = nullptr;
};

} // namespace ClassMngr::Next::Platform
