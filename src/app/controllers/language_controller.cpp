#include "language_controller.h"

#include "app/mainwindow.h"
#include "core/fontmanager.h"
#include "ui/shared/actions/action_registry.h"

#include <QApplication>

#include <optional>
#include <utility>

LanguageController::LanguageController(
    LanguageService& languageService,
    MainWindow* window,
    QObject* parent
    )
    : QObject(parent)
    , m_languagePreferencePort(languageService)
    , m_window(window)
{
}

void LanguageController::connectActions(
    ActionRegistry& actions
    )
{
    if (!actions.languageState)
    {
        return;
    }

    const auto previousLanguageHandler =
        actions.languageState->onChanged;

    actions.languageState->onChanged =
        [this, previousLanguageHandler](Language language)
    {
        if (previousLanguageHandler)
        {
            previousLanguageHandler(language);
        }

        const auto preference = languagePreferenceFor(language);
        if (!preference.has_value())
        {
            return;
        }

        if (!changeLanguage(*preference))
        {
            return;
        }

        if (qApp)
        {
            FontManager::applyGlobalFont(
                *qApp,
                m_languagePreferencePort.loadedLocaleName()
                );
        }

        if (m_window)
        {
            m_window->retranslateUi();
        }
    };

    const auto preference = languagePreferenceFor(
        actions.languageState->current()
        );
    if (preference.has_value())
    {
        (void) m_preferencesState.setLanguagePreference(*preference);
    }
}

ClassMngr::Next::Domain::Result<void> LanguageController::changeLanguage(
    const ClassMngr::Next::Application::LanguagePreference preference
    )
{
    auto nextPreferencesState = m_preferencesState;
    const auto stateResult = nextPreferencesState.setLanguagePreference(
        preference
        );
    if (!stateResult)
    {
        return stateResult;
    }

    const auto presentationResult = m_languagePreferencePort.apply(preference);
    if (!presentationResult)
    {
        return presentationResult;
    }

    m_preferencesState = std::move(nextPreferencesState);
    return ClassMngr::Next::Domain::Result<void>::success();
}

ClassMngr::Next::Application::UserPreferencesSnapshot
LanguageController::preferencesSnapshot() const
{
    return m_preferencesState.snapshot();
}

std::optional<ClassMngr::Next::Application::LanguagePreference>
LanguageController::languagePreferenceFor(
    const Language language
    )
{
    switch (language)
    {
    case Language::SystemDefault:
        return ClassMngr::Next::Application::LanguagePreference::SystemDefault;

    case Language::English:
        return ClassMngr::Next::Application::LanguagePreference::English;

    case Language::Korean:
        return ClassMngr::Next::Application::LanguagePreference::Korean;
    }

    return std::nullopt;
}
