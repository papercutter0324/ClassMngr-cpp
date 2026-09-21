#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/calendar_event_type_color_preferences.h"

#include <QByteArray>
#include <QDebug>
#include <QString>

#include <cstddef>
#include <string>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for dynamic calendar event-type colors. The caller
// supplies a normalized event type and a hex-RGB string; this adapter
// owns only the exact dynamic key and settings persistence.
class ApplicationServicesCalendarEventTypeColorPreferencesPort final
    : public Application::CalendarEventTypeColorPreferencesPort
{
public:
    explicit ApplicationServicesCalendarEventTypeColorPreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_settingsService(services.settingsService())
    {
    }

    explicit ApplicationServicesCalendarEventTypeColorPreferencesPort(
        SettingsService* settingsService
        ) noexcept
        : m_settingsService(settingsService)
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
        const std::string& normalizedEventType
        ) const override
    {
        if (!m_settingsService || !m_settingsService->isAvailable())
        {
            return {};
        }

        const QByteArray storedColor =
            m_settingsService
                ->loadOrDefault(
                    key(normalizedEventType),
                    QString()
                    )
                .toString()
                .toUtf8();

        return std::string(
            storedColor.constData(),
            static_cast<std::size_t>(storedColor.size())
            );
    }

    void write(
        const std::string& normalizedEventType,
        const std::string& colorHexRgb
        ) const override
    {
        if (!m_settingsService || !m_settingsService->isAvailable())
        {
            return;
        }

        const QString storedColor = QString::fromUtf8(
            colorHexRgb.data(),
            static_cast<qsizetype>(colorHexRgb.size())
            );
        if (const Status saved = m_settingsService->save(
                key(normalizedEventType),
                storedColor
                ); !saved)
        {
            qWarning()
                << "Failed to save calendar event type color:"
                << saved.error();
        }
    }

private:
    [[nodiscard]] static QString key(
        const std::string& normalizedEventType
        )
    {
        return QStringLiteral("calendar/eventTypeColor/")
            + QString::fromUtf8(
                normalizedEventType.data(),
                static_cast<qsizetype>(normalizedEventType.size())
                );
    }

    SettingsService* m_settingsService = nullptr;
};

} // namespace ClassMngr::Next::Platform
