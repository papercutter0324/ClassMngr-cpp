#include "class_navigation_preferences.h"

#include "data/data_service.h"

namespace
{
const QString VisibilityScopeKey =
    QStringLiteral("classes_navigation_visibility_scope");

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

ClassTabNavigation::VisibilityScope load(
    DataService* dataService
    )
{
    if (!dataService || !dataService->isOpen())
    {
        return ClassTabNavigation::VisibilityScope::ActiveSchedule;
    }

    const QVariant storedScope =
        dataService->loadSetting(VisibilityScopeKey, QVariant());
    const ClassTabNavigation::VisibilityScope visibilityScope =
        scopeFromSetting(storedScope);

    if (!storedScope.isValid())
    {
        save(dataService, visibilityScope);
    }

    return visibilityScope;
}

void save(
    DataService* dataService,
    ClassTabNavigation::VisibilityScope visibilityScope
    )
{
    if (!dataService || !dataService->isOpen())
    {
        return;
    }

    dataService->saveSetting(
        VisibilityScopeKey,
        settingValue(visibilityScope)
        );
}

}
