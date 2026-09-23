#pragma once

#include <QColor>
#include <QString>
#include <QStringList>

#include "next/application/custom_color_palette_preferences.h"

#include <optional>

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

    // Returns no value for malformed colors rather than substituting a
    // display default. Domain validation can therefore distinguish invalid
    // persisted/input values from intentional defaults.
    [[nodiscard]] static std::optional<QString> canonicalHexColor(
        const QString& color
        );

    static QColor getColor(
        const QColor& initialColor,
        QWidget* parent,
        const QString& title,
        const ClassMngr::Next::Application::
            CustomColorPalettePreferencesPort& palettePreferencesPort
        );

    // =====================================================
    // QColorDialog Custom Colors
    // =====================================================

    static void loadCustomColors(
        const ClassMngr::Next::Application::
            CustomColorPalettePreferencesPort& palettePreferencesPort
        );
    static void saveCustomColors(
        const ClassMngr::Next::Application::
            CustomColorPalettePreferencesPort& palettePreferencesPort
        );
};
