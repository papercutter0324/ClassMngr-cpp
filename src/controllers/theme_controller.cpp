#include "theme_controller.h"

#include "services/theme_service.h"
#include "ui/actions/action_registry.h"

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
        [this, previousThemeHandler](Theme theme)
    {
        if (previousThemeHandler)
        {
            previousThemeHandler(theme);
        }

        changeTheme(theme);
    };

    changeTheme(
        actions.themeState->current()
        );
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
