#pragma once

#include "ui/shared/constants/options.h"

#include <QObject>

class ActionRegistry;
class ThemeService;

class ThemeController : public QObject
{
    Q_OBJECT

public:
    explicit ThemeController(
        ThemeService* themeService,
        QObject* parent = nullptr
        );

    void connectActions(
        ActionRegistry& actions
        );

    void changeTheme(
        Theme theme
        );

private:
    ThemeService* m_themeService = nullptr;
};
