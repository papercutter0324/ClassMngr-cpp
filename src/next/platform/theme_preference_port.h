#pragma once

#include "core/theme_service.h"
#include "next/application/user_preferences_state.h"

namespace ClassMngr::Next::Platform
{

// Explicit Qt-facing adapter for the application theme preference. The
// application state remains typed and Qt-free; only this boundary knows the
// legacy Theme representation and applies it to ThemeService.
class ThemePreferencePort final
{
public:
    explicit ThemePreferencePort(
        ThemeService& themeService
        ) noexcept
        : m_themeService(themeService)
    {
    }

    ThemePreferencePort(
        const ThemePreferencePort&
        ) = delete;
    ThemePreferencePort& operator=(
        const ThemePreferencePort&
        ) = delete;
    ThemePreferencePort(
        ThemePreferencePort&&
        ) = delete;
    ThemePreferencePort& operator=(
        ThemePreferencePort&&
        ) = delete;

    [[nodiscard]] Domain::Result<void> apply(
        const Application::ThemePreference preference
        ) const
    {
        Theme theme;
        switch (preference)
        {
        case Application::ThemePreference::SystemDefault:
            theme = Theme::SystemDefault;
            break;

        case Application::ThemePreference::Light:
            theme = Theme::Light;
            break;

        case Application::ThemePreference::Dark:
            theme = Theme::Dark;
            break;

        default:
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message =
                    "Theme preference must be SystemDefault, Light, or Dark.",
                .recoverable = false
            });
        }

        m_themeService.setTheme(theme);
        return Domain::Result<void>::success();
    }

private:
    ThemeService& m_themeService;
};

} // namespace ClassMngr::Next::Platform
