#pragma once

class SettingsService;

struct ScheduleSettingsValues
{
    bool use24HourTime = false;
    bool showEnglishNames = false;
    bool showWeekends = false;
    bool showAllIntensiveHours = false;
    bool testingAffectsM1 = false;
};

namespace ScheduleSettingsPreferences
{

[[nodiscard]] ScheduleSettingsValues load(
    SettingsService* settingsService
    );

void save(
    SettingsService* settingsService,
    const ScheduleSettingsValues& values
    );

} // namespace ScheduleSettingsPreferences
