#pragma once

#include "features/classes/models/class_tab_navigation_model.h"

class SettingsService;

namespace ClassNavigationPreferences
{

[[nodiscard]] ClassTabNavigation::VisibilityScope load(
    SettingsService* settingsService
    );

void save(
    SettingsService* settingsService,
    ClassTabNavigation::VisibilityScope visibilityScope
    );

}
