#pragma once

#include "ui/shared/constants/options.h"

#include <QObject>
#include <QPalette>

class ThemeService : public QObject
{
    Q_OBJECT

public:
    explicit ThemeService(
        QObject* parent = nullptr
        );

    Theme currentTheme() const;

    void setTheme(
        Theme theme
        );

    void notifyThemeChanged();

signals:
    void themeChanged(
        Theme theme
        );

    void themeChanged();

private:
    QString stylesheetPath(
        Theme theme
        ) const;

    QString themeKey(
        Theme theme
        ) const;

    QString loadStylesheet(
        Theme theme
        ) const;

    QPalette buildPalette(
        Theme theme
        ) const;

    void refreshThemeProperties(
        const QString& themeName
        ) const;

private:
    Theme m_currentTheme = Theme::Dark;
    bool m_hasAppliedTheme = false;
};
