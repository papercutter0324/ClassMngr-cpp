#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/calendar_event_type_color_preferences.h"

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for dynamic calendar event-type colors. The caller
// supplies a typed event type and a hex-RGB string; this adapter
// owns only the exact dynamic key and settings persistence.
class ApplicationServicesCalendarEventTypeColorPreferencesPort final
    : public Application::CalendarEventTypeColorPreferencesPort
{
public:
    explicit ApplicationServicesCalendarEventTypeColorPreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_session(services.databaseSession())
    {
    }

    explicit ApplicationServicesCalendarEventTypeColorPreferencesPort(
        ApplicationServices* services
        ) noexcept
        : m_session(services ? services->databaseSession() : nullptr)
    {
    }

    ApplicationServicesCalendarEventTypeColorPreferencesPort(
        const ApplicationServicesCalendarEventTypeColorPreferencesPort&
        ) = delete;
    ApplicationServicesCalendarEventTypeColorPreferencesPort& operator=(
        const ApplicationServicesCalendarEventTypeColorPreferencesPort&
        ) = delete;
    ApplicationServicesCalendarEventTypeColorPreferencesPort(
        ApplicationServicesCalendarEventTypeColorPreferencesPort&&
        ) = delete;
    ApplicationServicesCalendarEventTypeColorPreferencesPort& operator=(
        ApplicationServicesCalendarEventTypeColorPreferencesPort&&
        ) = delete;

    [[nodiscard]] std::string read(
        const Domain::CalendarEventType eventType
        ) const override
    {
        if (!m_session || !m_session->isOpen())
        {
            return {};
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return {};
        }

        const auto setting = repository->loadSetting(key(eventType));
        const QByteArray storedColor = setting
            ? setting->toString().toUtf8()
            : QByteArray();

        return std::string(
            storedColor.constData(),
            static_cast<std::size_t>(storedColor.size())
            );
    }

    [[nodiscard]] Application::
        CalendarEventTypeColorPreferencesPort::SaveResult write(
        const Domain::CalendarEventType eventType,
        const std::string& colorHexRgb
        ) const override
    {
        if (!m_session || !m_session->isOpen())
        {
            return failure(
                "Calendar event type color could not be saved because the database session is unavailable."
                );
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return failure(
                "Calendar event type color could not be saved because the settings repository is unavailable."
                );
        }

        const QString storedColor = QString::fromUtf8(
            colorHexRgb.data(),
            static_cast<qsizetype>(colorHexRgb.size())
            );
        if (const Status saved = repository->saveSetting(
                key(eventType),
                storedColor
                ); !saved)
        {
            const QByteArray errorBytes = saved.error().toUtf8();
            return failure(
                errorBytes.isEmpty()
                    ? "Calendar event type color could not be saved."
                    : errorBytes.toStdString()
                );
        }

        return Application::
            CalendarEventTypeColorPreferencesPort::SaveResult::success();
    }

private:
    [[nodiscard]] static Application::
        CalendarEventTypeColorPreferencesPort::SaveResult failure(
            std::string message
            )
    {
        return Application::
            CalendarEventTypeColorPreferencesPort::SaveResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = std::move(message),
                .recoverable = false
            });
    }

    [[nodiscard]] static QString key(
        const Domain::CalendarEventType eventType
        )
    {
        const std::string_view eventTypeName =
            Domain::calendarEventTypeName(eventType);
        return QStringLiteral("calendar/eventTypeColor/")
            + QString::fromUtf8(
                eventTypeName.data(),
                static_cast<qsizetype>(eventTypeName.size())
                );
    }

    DatabaseSession* m_session = nullptr;
};

} // namespace ClassMngr::Next::Platform
