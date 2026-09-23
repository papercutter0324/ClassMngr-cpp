#include "colorutils.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <QColor>
#include <QColorDialog>
#include <QDialog>
#include <QRegularExpression>

namespace
{
double linearizedSrgbChannel(int channel)
{
    const double srgb = channel / 255.0;

    return srgb <= 0.04045
        ? srgb / 12.92
        : std::pow((srgb + 0.055) / 1.055, 2.4);
}

double relativeLuminance(const QColor& color)
{
    return (0.2126 * linearizedSrgbChannel(color.red()))
        + (0.7152 * linearizedSrgbChannel(color.green()))
        + (0.0722 * linearizedSrgbChannel(color.blue()));
}

void applyCustomColors(
    const QStringList& colors
    )
{
    for (
        int i = 0;
        i < std::min(
            static_cast<int>(colors.size()),
            ColorUtils::CUSTOM_COLOR_COUNT
            );
        ++i
        )
    {
        QColorDialog::setCustomColor(
            i,
            QColor(colors[i])
            );
    }
}

QStringList colorsForDialog(
    const ClassMngr::Next::Application::CustomColorPalette& palette
    )
{
    QStringList colors;
    for (const std::string& color : palette.hexColors)
    {
        colors.append(QString::fromUtf8(
            color.data(),
            static_cast<qsizetype>(color.size())
            ));
    }
    return colors;
}

ClassMngr::Next::Application::CustomColorPalette paletteFromDialog()
{
    auto palette =
        ClassMngr::Next::Application::defaultCustomColorPalette();
    for (std::size_t index = 0;
         index < ClassMngr::Next::Application::CustomColorPalette::EntryCount;
         ++index)
    {
        const QColor color = QColorDialog::customColor(
            static_cast<int>(index)
            );
        if (color.isValid())
        {
            palette.hexColors[index] = color.name(
                QColor::HexRgb
                ).toStdString();
        }
    }
    return palette;
}
}

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

    const double luminance = relativeLuminance(color);
    const double blackContrast =
        (luminance + 0.05) / 0.05;
    const double whiteContrast =
        1.05 / (luminance + 0.05);

    return blackContrast >= whiteContrast
        ? "#000000"
        : "#FFFFFF";
}

std::optional<QString> ColorUtils::canonicalHexColor(
    const QString& color
    )
{
    static const QRegularExpression hexColorExpression(
        QStringLiteral("^#[0-9A-Fa-f]{6}$")
        );

    const QString normalized = color.trimmed();
    if (!hexColorExpression.match(normalized).hasMatch())
    {
        return std::nullopt;
    }

    return normalized.toUpper();
}

QColor ColorUtils::getColor(
    const QColor& initialColor,
    QWidget* parent,
    const QString& title,
    const ClassMngr::Next::Application::
        CustomColorPalettePreferencesPort& palettePreferencesPort
    )
{
    loadCustomColors(palettePreferencesPort);
    QColor startingColor = initialColor.isValid()
        ? initialColor
        : QColor(QStringLiteral("#FFFFFF"));
    QColorDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setCurrentColor(startingColor);
    dialog.setOption(QColorDialog::DontUseNativeDialog, true);
    const int result = dialog.exec();
    saveCustomColors(palettePreferencesPort);
    return result == QDialog::Accepted ? dialog.selectedColor() : QColor{};
}

// =====================================================
// Load Custom Colors
// =====================================================

void ColorUtils::loadCustomColors(
    const ClassMngr::Next::Application::
        CustomColorPalettePreferencesPort& palettePreferencesPort
    )
{
    applyCustomColors(
        colorsForDialog(palettePreferencesPort.read())
        );
}

void ColorUtils::saveCustomColors(
    const ClassMngr::Next::Application::
        CustomColorPalettePreferencesPort& palettePreferencesPort
    )
{
    palettePreferencesPort.write(paletteFromDialog());
}
