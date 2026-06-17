#pragma once

#include "ui/shared/constants/options.h"

#include <QObject>

class ActionRegistry;
class LanguageService;
class MainWindow;

class LanguageController : public QObject
{
    Q_OBJECT

public:
    explicit LanguageController(
        LanguageService* languageService,
        MainWindow* window,
        QObject* parent = nullptr
        );

    void connectActions(
        ActionRegistry& actions
        );

    void changeLanguage(
        Language language
        );

private:
    LanguageService* m_languageService = nullptr;
    MainWindow* m_window = nullptr;
};
