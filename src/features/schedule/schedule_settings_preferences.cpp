#include "schedule_settings_preferences.h"

#include "app/services/feature_services.h"

#include <QDebug>

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

    if (const Status saved = settingsService->saveAll({
            {Use24HourTime, storedBool(values.use24HourTime)},
            {
                ShowKoreanTeacherEnglishNames,
                storedBool(values.showEnglishNames)
            },
            {ShowWeekends, storedBool(values.showWeekends)},
            {ShowAllHours, storedBool(values.showAllIntensiveHours)},
            {TestingAffectsM1, storedBool(values.testingAffectsM1)}
        }); !saved)
    {
        qWarning() << "Failed to save schedule settings:" << saved.error();
    }
}

} // namespace ScheduleSettingsPreferences
