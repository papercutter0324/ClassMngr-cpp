#include "colorutils.h"
#include "data/data_service.h"

#include <algorithm>
#include <array>
#include <QColor>
#include <QColorDialog>
#include <QDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QVariant>

namespace
{
constexpr auto CustomColorsSettingKey = "custom_colors";

const std::array<QString, ColorUtils::CUSTOM_COLOR_COUNT> DefaultCustomColors =
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

QStringList defaultCustomColors()
{
    return QStringList(
        DefaultCustomColors.begin(),
        DefaultCustomColors.end()
        );
}

QStringList colorsFromJson(
    const QString& text
    )
{
    QJsonParseError error;

    const QJsonDocument document =
        QJsonDocument::fromJson(
            text.toUtf8(),
            &error
            );

    if (error.error != QJsonParseError::NoError
        || !document.isArray())
    {
        return {};
    }

    QStringList colors;

    for (const QJsonValue& value : document.array())
    {
        colors.append(
            value.toString()
            );
    }

    return colors;
}

QStringList colorsFromSetting(
    const QVariant& value
    )
{
    const QStringList listValue =
        value.toStringList();

    if (listValue.size() > 1
        || (listValue.size() == 1
            && QColor(listValue.first()).isValid()))
    {
        return listValue;
    }

    const QString text =
        value.toString().trimmed();

    if (text.isEmpty())
    {
        return {};
    }

    const QStringList jsonColors =
        colorsFromJson(text);

    if (!jsonColors.isEmpty())
    {
        return jsonColors;
    }

    const QChar separator =
        text.contains('\n')
            ? QChar('\n')
            : (text.contains(';')
                   ? QChar(';')
                   : QChar(','));

    return text.split(
        separator,
        Qt::SkipEmptyParts
        );
}

QStringList normalizeCustomColors(
    const QStringList& source
    )
{
    QStringList colors =
        defaultCustomColors();

    for (
        int i = 0;
        i < std::min(
            static_cast<int>(source.size()),
            ColorUtils::CUSTOM_COLOR_COUNT
            );
        ++i
        )
    {
        const QColor color(source[i].trimmed());

        if (color.isValid())
        {
            colors[i] =
                color.name(QColor::HexRgb);
        }
    }

    return colors;
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

QString serializeCustomColors(
    const QStringList& colors
    )
{
    QJsonArray array;

    for (const QString& color : colors)
    {
        array.append(color);
    }

    return QString::fromUtf8(
        QJsonDocument(array).toJson(QJsonDocument::Compact)
        );
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

QColor ColorUtils::getColor(
    const QColor& initialColor,
    QWidget* parent,
    const QString& title,
    DataService* ds
    )
{
    loadCustomColors(ds);

    QColor startingColor =
        initialColor;

    if (!startingColor.isValid())
    {
        startingColor = QColor("#FFFFFF");
    }

    QColorDialog dialog(parent);

    dialog.setWindowTitle(title);
    dialog.setCurrentColor(startingColor);
    dialog.setOption(
        QColorDialog::DontUseNativeDialog,
        true
        );

    const int result =
        dialog.exec();

    saveCustomColors(ds);

    if (result != QDialog::Accepted)
    {
        return {};
    }

    return dialog.selectedColor();
}

// =====================================================
// Load Custom Colors
// =====================================================

void ColorUtils::loadCustomColors(
    DataService *ds
    )
{
    QStringList colors =
        defaultCustomColors();

    if (ds)
    {
        colors =
            normalizeCustomColors(
                colorsFromSetting(
                    ds->loadSetting(
                        CustomColorsSettingKey,
                        QString()
                        )
                    )
                );
    }

    applyCustomColors(colors);
}

// =====================================================
// Save Custom Colors
// =====================================================

void ColorUtils::saveCustomColors(
    DataService *ds
    )
{
    if (!ds)
    {
        return;
    }

    QStringList colors;

    for (int i = 0;
         i < CUSTOM_COLOR_COUNT;
         ++i)
    {
        const QColor color =
            QColorDialog::customColor(i);

        colors.append(
            color.isValid()
                ? color.name(QColor::HexRgb)
                : DefaultCustomColors[i]
            );
    }

    ds->saveSetting(
        CustomColorsSettingKey,
        serializeCustomColors(colors)
        );
}
