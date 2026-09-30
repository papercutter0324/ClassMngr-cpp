#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/academic_calendar_schedule_preferences.h"

#include <QByteArray>
#include <QDebug>
#include <QString>

#include <cstddef>
#include <string>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the opaque academic-calendar schedule payload. The
// adapter owns only settings availability, the exact legacy key, and UTF-8 /
// QVariant conversion; schedule JSON remains a feature-owned concern.
class ApplicationServicesAcademicCalendarSchedulePreferencesPort final
    : public Application::AcademicCalendarSchedulePreferencesPort
{
public:
    explicit ApplicationServicesAcademicCalendarSchedulePreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_session(services.databaseSession())
    {
    }

    explicit ApplicationServicesAcademicCalendarSchedulePreferencesPort(
        ApplicationServices* services
        ) noexcept
        : m_session(services ? services->databaseSession() : nullptr)
    {
    }

    ApplicationServicesAcademicCalendarSchedulePreferencesPort(
        const ApplicationServicesAcademicCalendarSchedulePreferencesPort&
        ) = delete;
    ApplicationServicesAcademicCalendarSchedulePreferencesPort& operator=(
        const ApplicationServicesAcademicCalendarSchedulePreferencesPort&
        ) = delete;
    ApplicationServicesAcademicCalendarSchedulePreferencesPort(
        ApplicationServicesAcademicCalendarSchedulePreferencesPort&&
        ) = delete;
    ApplicationServicesAcademicCalendarSchedulePreferencesPort& operator=(
        ApplicationServicesAcademicCalendarSchedulePreferencesPort&&
        ) = delete;

    [[nodiscard]] std::string read() const override
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

        const auto stored = repository->loadSetting(key());
        if (!stored)
        {
            return {};
        }

        const QByteArray payload = stored->toString().toUtf8();

        return std::string(
            payload.constData(),
            static_cast<std::size_t>(payload.size())
            );
    }

    void write(const std::string& payload) const override
    {
        if (!m_session || !m_session->isOpen())
        {
            return;
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return;
        }

        const QString storedPayload = QString::fromUtf8(
            payload.data(),
            static_cast<qsizetype>(payload.size())
            );
        if (const Status saved = repository->saveSetting(
                key(),
                storedPayload
                ); !saved)
        {
            qWarning()
                << "Failed to save academic calendar schedule:"
                << saved.error();
        }
    }

private:
    [[nodiscard]] static QString key()
    {
        return QStringLiteral("calendar/academicSchedule/v1");
    }

    DatabaseSession* m_session = nullptr;
};

} // namespace ClassMngr::Next::Platform
