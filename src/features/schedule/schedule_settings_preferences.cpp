#include "schedule_settings_preferences.h"

#include "app/services/feature_services.h"

namespace
{
const QString Use24HourTime =
    QStringLiteral("schedule_use_24h");
const QString ShowKoreanTeacherEnglishNames =
    QStringLiteral("schedule_show_korean_teacher_english_names");
const QString ShowWeekends =
    QStringLiteral("schedule_show_weekends");
const QString ShowAllHours =
    QStringLiteral("schedule_show_all_hours_v2");
const QString TestingAffectsM1 =
    QStringLiteral("schedule_testing_affects_m1");

bool settingToBool(
    const QVariant& value,
    bool defaultValue
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

QString storedBool(bool value)
{
    return value
        ? QStringLiteral("true")
        : QStringLiteral("false");
}
}

namespace ScheduleSettingsPreferences
{

ScheduleSettingsValues load(SettingsService* settingsService)
{
    if (!settingsService || !settingsService->isAvailable())
    {
        return {};
    }

    return {
        settingToBool(
            settingsService->loadOrDefault(Use24HourTime, QStringLiteral("false")),
            false
            ),
        settingToBool(
            settingsService->loadOrDefault(
                ShowKoreanTeacherEnglishNames,
                QStringLiteral("false")
                ),
            false
            ),
        settingToBool(
            settingsService->loadOrDefault(ShowWeekends, QStringLiteral("false")),
            false
            ),
        settingToBool(
            settingsService->loadOrDefault(ShowAllHours, QStringLiteral("false")),
            false
            ),
        settingToBool(
            settingsService->loadOrDefault(
                TestingAffectsM1,
                QStringLiteral("false")
                ),
            false
            )
    };
}

void save(
    SettingsService* settingsService,
    const ScheduleSettingsValues& values
    )
{
    if (!settingsService || !settingsService->isAvailable())
    {
        return;
    }

    settingsService->save(
        Use24HourTime,
        storedBool(values.use24HourTime)
        );
    settingsService->save(
        ShowKoreanTeacherEnglishNames,
        storedBool(values.showEnglishNames)
        );
    settingsService->save(
        ShowWeekends,
        storedBool(values.showWeekends)
        );
    settingsService->save(
        ShowAllHours,
        storedBool(values.showAllIntensiveHours)
        );
    settingsService->save(
        TestingAffectsM1,
        storedBool(values.testingAffectsM1)
        );
}

} // namespace ScheduleSettingsPreferences
