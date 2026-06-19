#include "language_controller.h"

#include "app/mainwindow.h"
#include "core/fontmanager.h"
#include "core/language_service.h"
#include "ui/shared/actions/action_registry.h"

#include <QApplication>

LanguageController::LanguageController(
    LanguageService* languageService,
    MainWindow* window,
    QObject* parent
    )
    : QObject(parent)
    , m_languageService(languageService)
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

        changeLanguage(language);
    };
}

void LanguageController::changeLanguage(
    Language language
    )
{
    if (!m_languageService)
    {
        return;
    }

    m_languageService->setLanguage(
        language
        );

    if (qApp)
    {
        FontManager::applyGlobalFont(
            *qApp,
            m_languageService->loadedLocaleName()
            );
    }

    if (m_window)
    {
        m_window->retranslateUi();
    }
}
