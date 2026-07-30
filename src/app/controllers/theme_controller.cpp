#include "theme_controller.h"

#include "core/theme_service.h"
#include "ui/shared/actions/action_registry.h"

ThemeController::ThemeController(
    ThemeService* themeService,
    QObject* parent
    )
    : QObject(parent)
    , m_themeService(themeService)
{
}

void ThemeController::connectActions(
    ActionRegistry& actions
    )
{
    if (!actions.themeState)
    {
        return;
    }

    const auto previousThemeHandler =
        actions.themeState->onChanged;

    actions.themeState->onChanged =
        [this, &actions, previousThemeHandler](Theme theme)
    {
        if (previousThemeHandler)
        {
            previousThemeHandler(theme);
        }

        changeTheme(theme);
        actions.refreshThemedIcons();
    };

    changeTheme(
        actions.themeState->current()
        );
    actions.refreshThemedIcons();
}

void ThemeController::changeTheme(
    Theme theme
    )
{
    if (!m_themeService)
    {
        return;
    }

    m_themeService->setTheme(theme);
}
