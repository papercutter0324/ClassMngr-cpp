#pragma once

#include "features/classes/models/class_tab_navigation_model.h"

class DataService;
class SettingsService;

namespace ClassNavigationPreferences
{

[[nodiscard]] ClassTabNavigation::VisibilityScope load(
    DataService* dataService
    );

void save(
    DataService* dataService,
    ClassTabNavigation::VisibilityScope visibilityScope
    );

[[nodiscard]] ClassTabNavigation::VisibilityScope load(
    SettingsService* settingsService
    );

void save(
    SettingsService* settingsService,
    ClassTabNavigation::VisibilityScope visibilityScope
    );

}
