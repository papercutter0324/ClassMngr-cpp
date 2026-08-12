#include "schedule_display_mode_preferences.h"

#include "data/data_service.h"

namespace
{
const QString LegacyShowIntensiveKey =
    QStringLiteral("schedule_show_intensive");

QString settingValue(
    ScheduleDisplayMode mode
    )
{
    switch (mode)
    {
    case ScheduleDisplayMode::Intensive:
        return QStringLiteral("intensive");

    case ScheduleDisplayMode::Testing:
        return QStringLiteral("testing");

    case ScheduleDisplayMode::Regular:
        return QStringLiteral("regular");
    }

    return QStringLiteral("regular");
}

bool settingToBool(
    const QVariant& value
    )
{
    if (!value.isValid())
    {
        return false;
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

ScheduleDisplayMode modeFromSetting(
    const QVariant& value,
    bool legacyIntensive
    )
{
    const QString normalized =
        value.toString().trimmed().toLower();

    if (normalized == QStringLiteral("intensive"))
    {
        return ScheduleDisplayMode::Intensive;
    }

    if (normalized == QStringLiteral("testing"))
    {
        return ScheduleDisplayMode::Testing;
    }

    if (normalized == QStringLiteral("regular"))
    {
        return ScheduleDisplayMode::Regular;
    }

    return legacyIntensive
        ? ScheduleDisplayMode::Intensive
        : ScheduleDisplayMode::Regular;
}
}

namespace ScheduleDisplayModePreferences
{

QString displayModeSettingKey()
{
    return QStringLiteral("schedule_display_mode");
}

ScheduleDisplayMode load(
    DataService* dataService
    )
{
    if (!dataService || !dataService->isOpen())
    {
        return ScheduleDisplayMode::Regular;
    }

    const QVariant storedMode =
        dataService->loadSetting(
            displayModeSettingKey(),
            QVariant()
            );
    const ScheduleDisplayMode mode =
        modeFromSetting(
            storedMode,
            settingToBool(
                dataService->loadSetting(
                    LegacyShowIntensiveKey,
                    QStringLiteral("false")
                    )
                )
            );

    if (!storedMode.isValid())
    {
        save(dataService, mode);
    }

    return mode;
}

void save(
    DataService* dataService,
    ScheduleDisplayMode mode
    )
{
    if (!dataService || !dataService->isOpen())
    {
        return;
    }

    dataService->saveSetting(
        displayModeSettingKey(),
        settingValue(mode)
        );
}

}
