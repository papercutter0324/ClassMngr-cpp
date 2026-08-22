#pragma once

#include <QColor>
#include <QString>
#include <QStringList>

#include <optional>

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
        SettingsService* settingsService
        );

    // =====================================================
    // QColorDialog Custom Colors
    // =====================================================

    static void loadCustomColors(SettingsService* settingsService);
    static void saveCustomColors(SettingsService* settingsService);
};
