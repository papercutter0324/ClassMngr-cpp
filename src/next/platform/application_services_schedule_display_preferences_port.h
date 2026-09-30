#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/schedule_display_preferences.h"

#include <QByteArray>
#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the schedule display preferences. The caller owns
// ApplicationServices; missing or unavailable legacy settings preserve the
// existing false fallback and saves are delegated atomically as one bundle.
class ApplicationServicesScheduleDisplayPreferencesPort final
    : public Application::ScheduleDisplayPreferencesPort
{
public:
    explicit ApplicationServicesScheduleDisplayPreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesScheduleDisplayPreferencesPort(
        const ApplicationServicesScheduleDisplayPreferencesPort&
        ) = delete;
    ApplicationServicesScheduleDisplayPreferencesPort& operator=(
        const ApplicationServicesScheduleDisplayPreferencesPort&
        ) = delete;
    ApplicationServicesScheduleDisplayPreferencesPort(
        ApplicationServicesScheduleDisplayPreferencesPort&&
        ) = delete;
    ApplicationServicesScheduleDisplayPreferencesPort& operator=(
        ApplicationServicesScheduleDisplayPreferencesPort&&
        ) = delete;

    [[nodiscard]] Application::ScheduleDisplayPreferencesResult load()
        const override
    {
        DatabaseSession* const session = m_services.databaseSession();
        if (!session || !session->isOpen())
        {
            return Application::ScheduleDisplayPreferencesResult::success({});
        }

        SettingsRepository* const repository = session->settingsRepository();
        if (!repository)
        {
            return Application::ScheduleDisplayPreferencesResult::success({});
        }

        const auto settingToBoolOrDefault = [repository](const QString& key)
        {
            const auto stored = repository->loadSetting(key);
            return stored
                ? settingToBool(*stored, false)
                : false;
        };

        return Application::ScheduleDisplayPreferencesResult::success({
            .use24HourTime = settingToBoolOrDefault(
                QStringLiteral("schedule_use_24h")
                ),
            .showEnglishNames = settingToBoolOrDefault(
                QStringLiteral(
                    "schedule_show_korean_teacher_english_names"
                    )
                ),
            .showWeekends = settingToBoolOrDefault(
                QStringLiteral("schedule_show_weekends")
                ),
            .showAllIntensiveHours = settingToBoolOrDefault(
                QStringLiteral("schedule_show_all_hours_v2")
                ),
            .testingAffectsM1 = settingToBoolOrDefault(
                QStringLiteral("schedule_testing_affects_m1")
                )
        });
    }

    [[nodiscard]] Application::ScheduleDisplayPreferencesSaveResult save(
        const Application::ScheduleDisplayPreferencesSaveRequest& request
        ) override
    {
        DatabaseSession* const session = m_services.databaseSession();
        if (!session || !session->isOpen())
        {
            return Application::ScheduleDisplayPreferencesSaveResult::success();
        }

        SettingsRepository* const repository = session->settingsRepository();
        if (!repository)
        {
            return Application::ScheduleDisplayPreferencesSaveResult::success();
        }

        const Status saved = repository->saveSettings({
            {
                QStringLiteral("schedule_use_24h"),
                storedBool(request.use24HourTime)
            },
            {
                QStringLiteral(
                    "schedule_show_korean_teacher_english_names"
                    ),
                storedBool(request.showEnglishNames)
            },
            {
                QStringLiteral("schedule_show_weekends"),
                storedBool(request.showWeekends)
            },
            {
                QStringLiteral("schedule_show_all_hours_v2"),
                storedBool(request.showAllIntensiveHours)
            },
            {
                QStringLiteral("schedule_testing_affects_m1"),
                storedBool(request.testingAffectsM1)
            }
        });
        if (!saved)
        {
            const QByteArray errorBytes = saved.error().toUtf8();
            return Application::ScheduleDisplayPreferencesSaveResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = errorBytes.isEmpty()
                    ? "Schedule display preferences could not be saved."
                    : errorBytes.toStdString(),
                .recoverable = false
            });
        }

        return Application::ScheduleDisplayPreferencesSaveResult::success();
    }

private:
    [[nodiscard]] static QString storedBool(const bool value)
    {
        return value
            ? QStringLiteral("true")
            : QStringLiteral("false");
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

        const QString text =
            value.toString().trimmed().toLower();

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

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
