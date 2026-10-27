#pragma once

#include "next/application/custom_color_palette_preferences.h"

#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <algorithm>
#include <cstddef>
#include <string>

namespace ClassMngr::Next::Platform
{

// Preserves the legacy setting key, accepted payload encodings, and 16-slot
// QColor normalization for every WorkspaceStore-backed palette adapter.
class CustomColorPalettePreferenceCodec final
{
public:
    [[nodiscard]] static QString key()
    {
        return QStringLiteral("custom_colors");
    }

    [[nodiscard]] static QStringList defaultColors()
    {
        const auto palette = Application::defaultCustomColorPalette();
        QStringList colors;
        for (const Application::CustomColorPaletteEntry& color
             : palette.hexColors)
        {
            colors.append(QString::fromStdString(color.value()));
        }
        return colors;
    }

    [[nodiscard]] static QStringList colorsFromSetting(
        const QVariant& value
        )
    {
        const QStringList listValue = value.toStringList();
        if (listValue.size() > 1
            || (listValue.size() == 1
                && QColor(listValue.first()).isValid()))
        {
            return listValue;
        }

        const QString text = value.toString().trimmed();
        if (text.isEmpty())
        {
            return {};
        }

        const QStringList jsonColors = colorsFromJson(text);
        if (!jsonColors.isEmpty())
        {
            return jsonColors;
        }

        const QChar separator = text.contains('\n')
            ? QChar('\n')
            : (text.contains(';') ? QChar(';') : QChar(','));
        return text.split(separator, Qt::SkipEmptyParts);
    }

    [[nodiscard]] static Application::CustomColorPalette normalizeStoredValue(
        const QVariant& value
        )
    {
        return toPalette(normalizeColors(colorsFromSetting(value)));
    }

    [[nodiscard]] static QStringList toQStringList(
        const Application::CustomColorPalette& palette
        )
    {
        QStringList colors;
        for (const Application::CustomColorPaletteEntry& color
             : palette.hexColors)
        {
            colors.append(QString::fromStdString(color.value()));
        }
        return colors;
    }

    [[nodiscard]] static QString serialize(
        const Application::CustomColorPalette& palette
        )
    {
        QJsonArray array;
        for (const QString& color : toQStringList(palette))
        {
            array.append(color);
        }
        return QString::fromUtf8(
            QJsonDocument(array).toJson(QJsonDocument::Compact)
            );
    }

private:
    [[nodiscard]] static QStringList colorsFromJson(const QString& text)
    {
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(
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
            colors.append(value.toString());
        }
        return colors;
    }

    [[nodiscard]] static QStringList normalizeColors(
        const QStringList& source
        )
    {
        QStringList colors = defaultColors();
        const qsizetype count = std::min(
            source.size(),
            static_cast<qsizetype>(
                Application::CustomColorPalette::EntryCount
                )
            );
        for (qsizetype index = 0; index < count; ++index)
        {
            const QColor color(source.at(index).trimmed());
            if (color.isValid())
            {
                colors[index] = color.name(QColor::HexRgb);
            }
        }
        return colors;
    }

    [[nodiscard]] static Application::CustomColorPalette toPalette(
        const QStringList& colors
        )
    {
        Application::CustomColorPalette palette =
            Application::defaultCustomColorPalette();
        for (std::size_t index = 0;
             index < Application::CustomColorPalette::EntryCount;
             ++index)
        {
            const QByteArray encoded = colors.at(
                static_cast<qsizetype>(index)
                ).toUtf8();
            palette.hexColors[index] =
                Application::CustomColorPaletteEntry(std::string(
                    encoded.constData(),
                    static_cast<std::size_t>(encoded.size())
                    ));
        }
        return palette;
    }
};

} // namespace ClassMngr::Next::Platform
