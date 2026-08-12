#pragma once

#include "features/classes/models/class_tab_navigation_model.h"

class DataService;

namespace ClassNavigationPreferences
{

[[nodiscard]] ClassTabNavigation::VisibilityScope load(
    DataService* dataService
    );

void save(
    DataService* dataService,
    ClassTabNavigation::VisibilityScope visibilityScope
    );

}
