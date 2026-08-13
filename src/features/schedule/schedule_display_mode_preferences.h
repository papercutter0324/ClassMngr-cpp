#pragma once

#include "features/schedule/ui/schedule_view_model.h"

#include <QString>
#include <QVariant>

class SettingsService;

namespace ScheduleDisplayModePreferences
{

[[nodiscard]] QString displayModeSettingKey();

[[nodiscard]] ScheduleDisplayMode load(SettingsService* settingsService);
void save(SettingsService* settingsService, ScheduleDisplayMode mode);

}
