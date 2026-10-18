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
// ApplicationServices; missing keys and unavailable sessions/repositories
// preserve the false load fallback, while read failures are typed. Saves
// require an open database and atomically persist the complete preference
// bundle.
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

        const auto loadBooleanSetting = [repository](const QString& key)
        {
            const auto stored = repository->loadSetting(key);
            if (!stored)
            {
                return Result<bool>(std::unexpected(
                    QStringLiteral("Loading schedule display preference '%1' failed: %2")
                        .arg(key, stored.error())
                    ));
            }

            return Result<bool>(stored->isValid()
                ? settingToBool(*stored, false)
                : false);
        };

        const auto use24HourTime = loadBooleanSetting(
            QStringLiteral("schedule_use_24h")
            );
        if (!use24HourTime)
        {
            return loadFailure(use24HourTime.error());
        }

        const auto showEnglishNames = loadBooleanSetting(
            QStringLiteral("schedule_show_korean_teacher_english_names")
            );
        if (!showEnglishNames)
        {
            return loadFailure(showEnglishNames.error());
        }

        const auto showWeekends = loadBooleanSetting(
            QStringLiteral("schedule_show_weekends")
            );
        if (!showWeekends)
        {
            return loadFailure(showWeekends.error());
        }

        const auto showAllIntensiveHours = loadBooleanSetting(
            QStringLiteral("schedule_show_all_hours_v2")
            );
        if (!showAllIntensiveHours)
        {
            return loadFailure(showAllIntensiveHours.error());
        }

        const auto testingAffectsM1 = loadBooleanSetting(
            QStringLiteral("schedule_testing_affects_m1")
            );
        if (!testingAffectsM1)
        {
            return loadFailure(testingAffectsM1.error());
        }

        return Application::ScheduleDisplayPreferencesResult::success({
            .use24HourTime = *use24HourTime,
            .showEnglishNames = *showEnglishNames,
            .showWeekends = *showWeekends,
            .showAllIntensiveHours = *showAllIntensiveHours,
            .testingAffectsM1 = *testingAffectsM1
        });
    }

    [[nodiscard]] Application::ScheduleDisplayPreferencesSaveResult save(
        const Application::ScheduleDisplayPreferencesSaveRequest& request
        ) override
    {
        DatabaseSession* const session = m_services.databaseSession();
        if (!session || !session->isOpen())
        {
            return Application::ScheduleDisplayPreferencesSaveResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message =
                    "Schedule display preferences require an open database.",
                .recoverable = false
            });
        }

        SettingsRepository* const repository = session->settingsRepository();
        if (!repository)
        {
            return Application::ScheduleDisplayPreferencesSaveResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message =
                    "Schedule display settings are unavailable.",
                .recoverable = false
            });
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

    [[nodiscard]] static Application::ScheduleDisplayPreferencesResult
    loadFailure(const QString& message)
    {
        const QByteArray errorBytes = message.toUtf8();
        return Application::ScheduleDisplayPreferencesResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = errorBytes.isEmpty()
                ? "Schedule display preferences could not be loaded."
                : errorBytes.toStdString(),
            .recoverable = false
        });
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
