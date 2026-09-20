#include "theme_controller.h"

#include "ui/shared/actions/action_registry.h"

#include <optional>

ThemeController::ThemeController(
    ThemeService& themeService,
    QObject* parent
    )
    : QObject(parent)
    , m_themePreferencePort(themeService)
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

        const auto preference = themePreferenceFor(theme);
        if (preference.has_value())
        {
            (void) changeTheme(*preference);
        }
        actions.refreshThemedIcons();
    };

    const auto preference = themePreferenceFor(
        actions.themeState->current()
        );
    if (preference.has_value())
    {
        (void) m_preferencesState.setThemePreference(*preference);
    }
}

ClassMngr::Next::Domain::Result<void> ThemeController::changeTheme(
    const ClassMngr::Next::Application::ThemePreference preference
    )
{
    const auto previousSnapshot = m_preferencesState.snapshot();
    const auto stateResult = m_preferencesState.setThemePreference(
        preference
        );
    if (!stateResult)
    {
        return stateResult;
    }

    const auto presentationResult = m_themePreferencePort.apply(preference);
    if (!presentationResult)
    {
        (void) m_preferencesState.setThemePreference(
            previousSnapshot.themePreference()
            );
        return presentationResult;
    }

    return ClassMngr::Next::Domain::Result<void>::success();
}

ClassMngr::Next::Application::UserPreferencesSnapshot
ThemeController::preferencesSnapshot() const
{
    return m_preferencesState.snapshot();
}

std::optional<ClassMngr::Next::Application::ThemePreference>
ThemeController::themePreferenceFor(
    const Theme theme
    )
{
    switch (theme)
    {
    case Theme::SystemDefault:
        return ClassMngr::Next::Application::ThemePreference::SystemDefault;

    case Theme::Light:
        return ClassMngr::Next::Application::ThemePreference::Light;

    case Theme::Dark:
        return ClassMngr::Next::Application::ThemePreference::Dark;
    }

    return std::nullopt;
}
