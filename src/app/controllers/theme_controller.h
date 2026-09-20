#pragma once

#include "next/application/user_preferences_state.h"
#include "next/platform/theme_preference_port.h"
#include "ui/shared/constants/options.h"

#include <QObject>

#include <optional>

class ActionRegistry;

class ThemeController : public QObject
{
    Q_OBJECT

public:
    explicit ThemeController(
        ThemeService& themeService,
        QObject* parent = nullptr
        );

    void connectActions(
        ActionRegistry& actions
        );

    [[nodiscard]] ClassMngr::Next::Domain::Result<void> changeTheme(
        ClassMngr::Next::Application::ThemePreference preference
        );

    [[nodiscard]] ClassMngr::Next::Application::UserPreferencesSnapshot
    preferencesSnapshot() const;

private:
    static std::optional<ClassMngr::Next::Application::ThemePreference>
    themePreferenceFor(
        Theme theme
        );

    ClassMngr::Next::Platform::ThemePreferencePort m_themePreferencePort;
    ClassMngr::Next::Application::UserPreferencesState m_preferencesState;
};
