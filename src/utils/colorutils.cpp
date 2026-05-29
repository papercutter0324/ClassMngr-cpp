#include "colorutils.h"

#include <algorithm>

// =====================================================
// Default Colors
// =====================================================

const std::array<QString, ColorUtils::CUSTOM_COLOR_COUNT>
    ColorUtils::DEFAULT_CUSTOM_COLORS =
    {
        "#F94144",
        "#F3722C",
        "#F8961E",
        "#F9C74F",
        "#90BE6D",
        "#43AA8B",
        "#577590",
        "#277DA1",
        "#9B5DE5",
        "#F15BB5",
        "#00BBF9",
        "#00F5D4",
        "#FFFFFF",
        "#D9D9D9",
        "#808080",
        "#000000",
};

// =====================================================
// Lighten
// =====================================================

QColor ColorUtils::lighten(
    const QColor &color,
    double factor
    )
{
    return QColor(
        std::min(
            static_cast<int>(color.red() * factor),
            255
            ),

        std::min(
            static_cast<int>(color.green() * factor),
            255
            ),

        std::min(
            static_cast<int>(color.blue() * factor),
            255
            )
        );
}

// =====================================================
// Soften
// =====================================================

QColor ColorUtils::soften(
    const QColor &color,
    double factor
    )
{
    return QColor(
        static_cast<int>(
            color.red()
            + (255 - color.red()) * factor
            ),

        static_cast<int>(
            color.green()
            + (255 - color.green()) * factor
            ),

        static_cast<int>(
            color.blue()
            + (255 - color.blue()) * factor
            )
        );
}

// =====================================================
// Contrasting Font Color
// =====================================================

QString ColorUtils::getContrastingFontColor(
    const QColor &color
    )
{
    if (!color.isValid())
    {
        return "#000000";
    }

    const int r = color.red();
    const int g = color.green();
    const int b = color.blue();

    const double luminance =
        (0.299 * r)
        + (0.587 * g)
        + (0.114 * b);

    return luminance > 186
               ? "#000000"
               : "#FFFFFF";
}

// =====================================================
// Load Custom Colors
// =====================================================

void ColorUtils::loadCustomColors(
    DataService *ds
    )
{
    QStringList savedColors =
        ds->loadSetting(
            "custom_colors",
            QStringList(
                DEFAULT_CUSTOM_COLORS.begin(),
                DEFAULT_CUSTOM_COLORS.end()
                )
            );

    for (int i = 0;
         i < std::min(
             savedColors.size(),
             CUSTOM_COLOR_COUNT
             );
         ++i)
    {
        QColorDialog::setCustomColor(
            i,
            QColor(savedColors[i])
            );
    }
}

// =====================================================
// Save Custom Colors
// =====================================================

void ColorUtils::saveCustomColors(
    DataService *ds
    )
{
    QStringList colors;

    for (int i = 0;
         i < CUSTOM_COLOR_COUNT;
         ++i)
    {
        colors.append(
            QColorDialog::customColor(i).name()
            );
    }

    ds->saveSetting(
        "custom_colors",
        colors
        );
}