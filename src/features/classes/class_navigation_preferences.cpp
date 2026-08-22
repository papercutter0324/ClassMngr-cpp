#include "class_navigation_preferences.h"

#include "app/services/feature_services.h"

namespace
{
const QString VisibilityScopeKey =
    QStringLiteral("classes_navigation_visibility_scope");
const QString DayFilterResetPolicyKey =
    QStringLiteral("classes_navigation_day_filter_reset_policy");
const QString ClassSelectionResetPolicyKey =
    QStringLiteral("classes_navigation_class_selection_reset_policy");

QString settingValue(
    ClassTabNavigation::VisibilityScope visibilityScope
    )
{
    return visibilityScope
        == ClassTabNavigation::VisibilityScope::AllClasses
        ? QStringLiteral("all_classes")
        : QStringLiteral("active_schedule");
}

ClassTabNavigation::VisibilityScope scopeFromSetting(
    const QVariant& value
    )
{
    return value.toString().trimmed().toLower()
        == QStringLiteral("all_classes")
        ? ClassTabNavigation::VisibilityScope::AllClasses
        : ClassTabNavigation::VisibilityScope::ActiveSchedule;
}
}

namespace ClassNavigationPreferences
{

ClassTabNavigation::VisibilityScope load(SettingsService* settingsService)
{
    if (!settingsService || !settingsService->isAvailable())
        return ClassTabNavigation::VisibilityScope::ActiveSchedule;

    const Result<QVariant> storedScopeResult =
        settingsService->load(VisibilityScopeKey);
    const QVariant storedScope = storedScopeResult.value_or(QVariant());
    const auto visibilityScope = scopeFromSetting(storedScope);
    if (!storedScope.isValid())
        save(settingsService, visibilityScope);
    return visibilityScope;
}

static QString resetPolicyValue(
    ClassNavigationPreferences::SessionResetPolicy policy
    )
{
    return policy == ClassNavigationPreferences::SessionResetPolicy::OnPageLeave
        ? QStringLiteral("on_page_leave")
        : QStringLiteral("on_application_close");
}

static ClassNavigationPreferences::SessionResetPolicy resetPolicyFromSetting(
    const QVariant& value
    )
{
    return value.toString().trimmed().toLower()
        == QStringLiteral("on_page_leave")
        ? ClassNavigationPreferences::SessionResetPolicy::OnPageLeave
        : ClassNavigationPreferences::SessionResetPolicy::OnApplicationClose;
}

void save(
    SettingsService* settingsService,
    ClassTabNavigation::VisibilityScope visibilityScope
    )
{
    if (settingsService && settingsService->isAvailable())
        static_cast<void>(
            settingsService->save(VisibilityScopeKey, settingValue(visibilityScope))
            );
}

SessionResetPolicy dayFilterResetPolicy(
    SettingsService* settingsService
    )
{
    if (!settingsService || !settingsService->isAvailable())
    {
        return SessionResetPolicy::OnApplicationClose;
    }

    const Result<QVariant> storedPolicyResult =
        settingsService->load(DayFilterResetPolicyKey);
    const QVariant storedPolicy = storedPolicyResult.value_or(QVariant());
    const SessionResetPolicy policy = resetPolicyFromSetting(storedPolicy);
    if (!storedPolicy.isValid())
    {
        saveDayFilterResetPolicy(settingsService, policy);
    }
    return policy;
}

void saveDayFilterResetPolicy(
    SettingsService* settingsService,
    SessionResetPolicy policy
    )
{
    if (settingsService && settingsService->isAvailable())
    {
        static_cast<void>(
            settingsService->save(
                DayFilterResetPolicyKey,
                resetPolicyValue(policy)
                )
            );
    }
}

SessionResetPolicy classSelectionResetPolicy(
    SettingsService* settingsService
    )
{
    if (!settingsService || !settingsService->isAvailable())
    {
        return SessionResetPolicy::OnApplicationClose;
    }

    const Result<QVariant> storedPolicyResult =
        settingsService->load(ClassSelectionResetPolicyKey);
    const QVariant storedPolicy = storedPolicyResult.value_or(QVariant());
    const SessionResetPolicy policy = resetPolicyFromSetting(storedPolicy);
    if (!storedPolicy.isValid())
    {
        saveClassSelectionResetPolicy(settingsService, policy);
    }
    return policy;
}

void saveClassSelectionResetPolicy(
    SettingsService* settingsService,
    SessionResetPolicy policy
    )
{
    if (settingsService && settingsService->isAvailable())
    {
        static_cast<void>(
            settingsService->save(
                ClassSelectionResetPolicyKey,
                resetPolicyValue(policy)
                )
            );
    }
}

}
