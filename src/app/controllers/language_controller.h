#pragma once

#include "next/application/user_preferences_state.h"
#include "next/platform/language_preference_port.h"
#include "ui/shared/constants/options.h"

#include <QObject>

#include <optional>

class ActionRegistry;
class MainWindow;

class LanguageController : public QObject
{
    Q_OBJECT

public:
    explicit LanguageController(
        LanguageService& languageService,
        MainWindow* window,
        QObject* parent = nullptr
        );

    void connectActions(
        ActionRegistry& actions
        );

    [[nodiscard]] ClassMngr::Next::Domain::Result<void> changeLanguage(
        ClassMngr::Next::Application::LanguagePreference preference
        );

    [[nodiscard]] ClassMngr::Next::Application::UserPreferencesSnapshot
    preferencesSnapshot() const;

private:
    static std::optional<ClassMngr::Next::Application::LanguagePreference>
    languagePreferenceFor(
        Language language
        );

    ClassMngr::Next::Platform::LanguagePreferencePort m_languagePreferencePort;
    ClassMngr::Next::Application::UserPreferencesState m_preferencesState;
    MainWindow* m_window = nullptr;
};
