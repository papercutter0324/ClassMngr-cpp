#pragma once

#include "ui/shared/constants/options.h"

#include <QObject>

class ActionRegistry;
class LanguageService;

class FontSizeController : public QObject
{
    Q_OBJECT

public:
    explicit FontSizeController(
        LanguageService* languageService,
        QObject* parent = nullptr
        );

    void connectActions(
        ActionRegistry& actions
        );

    void changeFontSize(
        FontSize fontSize
        );

private:
    LanguageService* m_languageService = nullptr;
};
