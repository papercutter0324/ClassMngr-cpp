#pragma once

#include "features/classes/models/class_tab_navigation_model.h"

class SettingsService;

namespace ClassNavigationPreferences
{

enum class SessionResetPolicy
{
    OnApplicationClose,
    OnPageLeave
};

[[nodiscard]] ClassTabNavigation::VisibilityScope load(
    SettingsService* settingsService
    );

void save(
    SettingsService* settingsService,
    ClassTabNavigation::VisibilityScope visibilityScope
    );

[[nodiscard]] bool showMiddleSchoolAnalyticsAndEvaluations(
    SettingsService* settingsService
    );

void saveShowMiddleSchoolAnalyticsAndEvaluations(
    SettingsService* settingsService,
    bool show
    );

[[nodiscard]] SessionResetPolicy dayFilterResetPolicy(
    SettingsService* settingsService
    );

void saveDayFilterResetPolicy(
    SettingsService* settingsService,
    SessionResetPolicy policy
    );

[[nodiscard]] SessionResetPolicy classSelectionResetPolicy(
    SettingsService* settingsService
    );

void saveClassSelectionResetPolicy(
    SettingsService* settingsService,
    SessionResetPolicy policy
    );

}
