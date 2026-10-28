#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/settings_repository.h"
#include "next/application/academic_calendar_schedule_preferences.h"

#include <QByteArray>
#include <QString>
#include <QVariant>

#include <cstddef>
#include <string>
#include <utility>

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
        : m_store(services.workspaceStore())
    {
    }

    explicit ApplicationServicesAcademicCalendarSchedulePreferencesPort(
        ApplicationServices* services
        ) noexcept
        : m_store(services ? services->workspaceStore() : nullptr)
    {
    }

    explicit ApplicationServicesAcademicCalendarSchedulePreferencesPort(
        WorkspaceStore& store
        ) noexcept
        : m_store(&store)
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

    [[nodiscard]] Application::AcademicCalendarSchedulePreferencesReadResult
    read() const override
    {
        if (!m_store || !m_store->isOpen())
        {
            return Application::AcademicCalendarSchedulePreferencesReadResult::
                success({});
        }

        SettingsRepository* const repository = m_store->settingsRepository();
        if (!repository)
        {
            return Application::AcademicCalendarSchedulePreferencesReadResult::
                success({});
        }

        const auto stored = repository->loadSetting(key());
        if (!stored)
        {
            const QByteArray errorBytes = toLegacyDiagnostic(stored.error()).toUtf8();
            return Application::AcademicCalendarSchedulePreferencesReadResult::
                failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = errorBytes.isEmpty()
                        ? "Academic calendar schedule could not be read."
                        : errorBytes.toStdString(),
                    .recoverable = false
                });
        }

        const QByteArray payload = stored->toString().toUtf8();

        return Application::AcademicCalendarSchedulePreferencesReadResult::
            success(
                Application::AcademicCalendarScheduleJsonText(
                    std::string(
                        payload.constData(),
                        static_cast<std::size_t>(payload.size())
                        )
                    )
                );
    }

    [[nodiscard]] Application::AcademicCalendarSchedulePreferencesSaveResult
    write(
        const Application::AcademicCalendarScheduleJsonText& payload
        ) const override
    {
        if (!m_store || !m_store->isOpen())
        {
            return failure(
                "Academic calendar schedule persistence is unavailable."
                );
        }

        SettingsRepository* const repository = m_store->settingsRepository();
        if (!repository)
        {
            return failure(
                "Academic calendar schedule persistence is unavailable."
                );
        }

        const std::string& payloadText = payload.value();
        const QString storedPayload = QString::fromUtf8(
            payloadText.data(),
            static_cast<qsizetype>(payloadText.size())
            );
        if (const RepositoryStatus saved = repository->saveSetting(
                key(),
                storedPayload
                ); !saved)
        {
            const QByteArray errorBytes = toLegacyDiagnostic(saved.error()).toUtf8();
            return failure(
                errorBytes.isEmpty()
                    ? "Academic calendar schedule could not be saved."
                    : errorBytes.toStdString()
                );
        }

        return Application::
            AcademicCalendarSchedulePreferencesSaveResult::success();
    }

private:
    [[nodiscard]] static Application::
    AcademicCalendarSchedulePreferencesSaveResult failure(
        std::string message
        )
    {
        return Application::AcademicCalendarSchedulePreferencesSaveResult::
            failure({
                .code = Domain::ErrorCode::Technical,
                .message = std::move(message),
                .recoverable = false
            });
    }

    [[nodiscard]] static QString key()
    {
        return QStringLiteral("calendar/academicSchedule/v1");
    }

    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform
