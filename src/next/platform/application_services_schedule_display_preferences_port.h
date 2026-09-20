#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
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
        const SettingsService* settingsService = m_services.settingsService();
        if (!settingsService || !settingsService->isAvailable())
        {
            return Application::ScheduleDisplayPreferencesResult::success({});
        }

        return Application::ScheduleDisplayPreferencesResult::success({
            .use24HourTime = settingToBool(
                settingsService->loadOrDefault(
                    QStringLiteral("schedule_use_24h"),
                    QStringLiteral("false")
                    ),
                false
                ),
            .showEnglishNames = settingToBool(
                settingsService->loadOrDefault(
                    QStringLiteral(
                        "schedule_show_korean_teacher_english_names"
                        ),
                    QStringLiteral("false")
                    ),
                false
                ),
            .showWeekends = settingToBool(
                settingsService->loadOrDefault(
                    QStringLiteral("schedule_show_weekends"),
                    QStringLiteral("false")
                    ),
                false
                ),
            .showAllIntensiveHours = settingToBool(
                settingsService->loadOrDefault(
                    QStringLiteral("schedule_show_all_hours_v2"),
                    QStringLiteral("false")
                    ),
                false
                ),
            .testingAffectsM1 = settingToBool(
                settingsService->loadOrDefault(
                    QStringLiteral("schedule_testing_affects_m1"),
                    QStringLiteral("false")
                    ),
                false
                )
        });
    }

    [[nodiscard]] Application::ScheduleDisplayPreferencesSaveResult save(
        const Application::ScheduleDisplayPreferencesSaveRequest& request
        ) override
    {
        const SettingsService* settingsService = m_services.settingsService();
        if (!settingsService || !settingsService->isAvailable())
        {
            return Application::ScheduleDisplayPreferencesSaveResult::success();
        }

        const Status saved = settingsService->saveAll({
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
