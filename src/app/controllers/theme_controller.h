#pragma once

#include "next/application/user_preferences_state.h"
#include "next/application/theme_preferences_port.h"
#include "next/platform/theme_preference_port.h"
#include "ui/shared/constants/options.h"

#include <QObject>

#include <optional>
#include <functional>

class ActionRegistry;

class ThemeController : public QObject
{
    Q_OBJECT

public:
    using FailureHandler = std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )>;

    explicit ThemeController(
        ThemeService& themeService,
        QObject* parent = nullptr,
        FailureHandler onSaveFailure = {}
        );

    ThemeController(
        ThemeService& themeService,
        const ClassMngr::Next::Application::ThemePreferencesPort& preferencesPort,
        FailureHandler onSaveFailure = {},
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
    // The injected port must outlive the controller.
    const ClassMngr::Next::Application::ThemePreferencesPort& m_preferencesPort;
    FailureHandler m_onSaveFailure;
    ClassMngr::Next::Application::UserPreferencesState m_preferencesState;
};
