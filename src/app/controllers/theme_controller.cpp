#include "theme_controller.h"

#include "ui/shared/actions/action_registry.h"
#include "next/platform/settings_manager_theme_preferences_port.h"

#include <optional>
#include <QScopedValueRollback>
#include <utility>

namespace
{
const ClassMngr::Next::Application::ThemePreferencesPort&
defaultThemePreferencesPort()
{
    static const ClassMngr::Next::Platform::SettingsManagerThemePreferencesPort port;
    return port;
}

Theme legacyThemeFor(
    const ClassMngr::Next::Application::ThemePreference preference
    )
{
    using ClassMngr::Next::Application::ThemePreference;
    switch (preference)
    {
    case ThemePreference::Dark:
        return Theme::Dark;
    case ThemePreference::Light:
        return Theme::Light;
    case ThemePreference::SystemDefault:
        return Theme::SystemDefault;
    }
    return Theme::SystemDefault;
}
}

ThemeController::ThemeController(
    ThemeService& themeService,
    QObject* parent,
    FailureHandler onSaveFailure
    )
    : ThemeController(
        themeService,
        defaultThemePreferencesPort(),
        std::move(onSaveFailure),
        parent
        )
{
}

ThemeController::ThemeController(
    ThemeService& themeService,
    const ClassMngr::Next::Application::ThemePreferencesPort& preferencesPort,
    FailureHandler onSaveFailure,
    QObject* parent
    )
    : QObject(parent)
    , m_themePreferencePort(themeService)
    , m_preferencesPort(preferencesPort)
    , m_onSaveFailure(std::move(onSaveFailure))
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
        if (m_restoringThemeSelection)
        {
            return;
        }

        const auto preference = themePreferenceFor(theme);
        if (preference.has_value())
        {
            const auto previousPreference =
                m_preferencesState.snapshot().themePreference();
            const auto result = changeTheme(*preference);
            if (!result)
            {
                {
                    const QScopedValueRollback restoring(
                        m_restoringThemeSelection,
                        true
                        );
                    actions.themeState->set(legacyThemeFor(previousPreference));
                }
                if (m_onSaveFailure)
                {
                    m_onSaveFailure(result.error());
                }
                return;
            }
        }
        if (previousThemeHandler)
        {
            previousThemeHandler(theme);
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
    auto candidateState = m_preferencesState;
    const auto stateResult = candidateState.setThemePreference(
        preference
        );
    if (!stateResult)
    {
        return stateResult;
    }

    ClassMngr::Next::Application::Theme persistedTheme;
    switch (preference)
    {
    case ClassMngr::Next::Application::ThemePreference::Dark:
        persistedTheme = ClassMngr::Next::Application::Theme::Dark;
        break;
    case ClassMngr::Next::Application::ThemePreference::Light:
        persistedTheme = ClassMngr::Next::Application::Theme::Light;
        break;
    case ClassMngr::Next::Application::ThemePreference::SystemDefault:
        persistedTheme = ClassMngr::Next::Application::Theme::SystemDefault;
        break;
    default:
        return ClassMngr::Next::Domain::Result<void>::failure({
            .code = ClassMngr::Next::Domain::ErrorCode::InvalidInput,
            .message = "Theme preference must be SystemDefault, Light, or Dark.",
            .recoverable = false
        });
    }

    const auto persistenceResult = m_preferencesPort.write(persistedTheme);
    if (!persistenceResult)
    {
        return persistenceResult;
    }

    const auto presentationResult = m_themePreferencePort.apply(preference);
    if (!presentationResult)
    {
        return presentationResult;
    }

    m_preferencesState = std::move(candidateState);
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
