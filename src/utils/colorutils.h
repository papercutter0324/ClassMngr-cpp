#pragma once

#include <QColor>
#include <QColorDialog>
#include <QString>
#include <QStringList>
#include <array>

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

    // =====================================================
    // QColorDialog Custom Colors
    // =====================================================

    static void loadCustomColors(
        class DataService *ds
        );

    static void saveCustomColors(
        class DataService *ds
        );

private:

    static const std::array<QString, CUSTOM_COLOR_COUNT>
        DEFAULT_CUSTOM_COLORS;
};