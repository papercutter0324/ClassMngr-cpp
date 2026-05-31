#pragma once

#include <QObject>

class ThemeService : public QObject
{
    Q_OBJECT

public:
    explicit ThemeService(
        QObject* parent = nullptr
        );

    void notifyThemeChanged();

signals:
    void themeChanged();
};