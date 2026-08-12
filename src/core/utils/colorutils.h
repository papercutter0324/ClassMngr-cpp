#pragma once

#include <QColor>
#include <QString>
#include <QStringList>

class DataService;
class SettingsService;
class QWidget;

class ColorUtils
{
public:

    // =====================================================
    // Constants
    // =====================================================

    static constexpr int CUSTOM_COLOR_COUNT = 16;

    // =====================================================
    // Color Helpers
    // =====================================================

    static QColor lighten(
        const QColor &color,
        double factor = 1.25
        );

    static QColor soften(
        const QColor &color,
        double factor = 0.4
        );

    static QString getContrastingFontColor(
        const QColor &color
        );

    static QColor getColor(
        const QColor& initialColor,
        QWidget* parent,
        const QString& title,
        DataService* ds
        );
    static QColor getColor(
        const QColor& initialColor,
        QWidget* parent,
        const QString& title,
        SettingsService* settingsService
        );

    // =====================================================
    // QColorDialog Custom Colors
    // =====================================================

    static void loadCustomColors(
        DataService *ds
        );

    static void saveCustomColors(
        DataService *ds
        );
    static void loadCustomColors(SettingsService* settingsService);
    static void saveCustomColors(SettingsService* settingsService);
};
