#include "font_size_controller.h"

#include "core/fontmanager.h"
#include "core/language_service.h"
#include "ui/shared/actions/action_registry.h"

#include <QApplication>

FontSizeController::FontSizeController(
    LanguageService* languageService,
    QObject* parent
    )
    : QObject(parent)
    , m_languageService(languageService)
{
}

void FontSizeController::connectActions(
    ActionRegistry& actions
    )
{
    if (!actions.fontSizeState)
    {
        return;
    }

    const auto previousFontSizeHandler =
        actions.fontSizeState->onChanged;

    actions.fontSizeState->onChanged =
        [this, previousFontSizeHandler](FontSize fontSize)
    {
        if (previousFontSizeHandler)
        {
            previousFontSizeHandler(fontSize);
        }

        changeFontSize(fontSize);
    };
}

void FontSizeController::changeFontSize(
    FontSize fontSize
    )
{
    if (!qApp)
    {
        return;
    }

    FontManager::applyFontSize(
        *qApp,
        m_languageService
            ? m_languageService->loadedLocaleName()
            : QString(),
        fontSizeOffset(fontSize)
        );
}
