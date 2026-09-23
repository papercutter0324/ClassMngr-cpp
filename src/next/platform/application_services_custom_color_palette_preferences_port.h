#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/custom_color_palette_preferences.h"

#include <QColor>
#include <QDebug>
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

// Qt-boundary adapter for the shared custom-color palette. It owns the exact
// legacy key, all supported stored payload formats, QColor canonicalization,
// fixed-size normalization, and the compatibility save warning.
class ApplicationServicesCustomColorPalettePreferencesPort final
    : public Application::CustomColorPalettePreferencesPort
{
public:
    explicit ApplicationServicesCustomColorPalettePreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_settingsService(services.settingsService())
    {
    }

    explicit ApplicationServicesCustomColorPalettePreferencesPort(
        ApplicationServices* services
        ) noexcept
        : m_settingsService(
              services
                  ? services->settingsService()
                  : nullptr
              )
    {
    }

    ApplicationServicesCustomColorPalettePreferencesPort(
        const ApplicationServicesCustomColorPalettePreferencesPort&
        ) = delete;
    ApplicationServicesCustomColorPalettePreferencesPort& operator=(
        const ApplicationServicesCustomColorPalettePreferencesPort&
        ) = delete;
    ApplicationServicesCustomColorPalettePreferencesPort(
        ApplicationServicesCustomColorPalettePreferencesPort&&
        ) = delete;
    ApplicationServicesCustomColorPalettePreferencesPort& operator=(
        ApplicationServicesCustomColorPalettePreferencesPort&&
        ) = delete;

    [[nodiscard]] Application::CustomColorPalette read()
        const override
    {
        if (!m_settingsService || !m_settingsService->isAvailable())
        {
            return Application::defaultCustomColorPalette();
        }

        return normalizeStoredValue(
            m_settingsService->loadOrDefault(key(), QString())
            );
    }

    void write(
        const Application::CustomColorPalette& palette
        ) const override
    {
        if (!m_settingsService || !m_settingsService->isAvailable())
        {
            return;
        }

        const QStringList colors = toQStringList(palette);
        if (const Status saved = m_settingsService->save(
                key(),
                serializeCustomColors(colors)
                ); !saved)
        {
            qWarning() << "Failed to save custom colors:" << saved.error();
        }
    }

    // Kept on the Qt-bound adapter so the legacy QVariant payload conversion
    // can be verified independently of the database driver's TEXT coercion.
    [[nodiscard]] static Application::CustomColorPalette normalizeStoredValue(
        const QVariant& value
        )
    {
        return toPalette(
            normalizeCustomColors(colorsFromSetting(value))
            );
    }

private:
    [[nodiscard]] static QStringList defaultCustomColors()
    {
        const auto defaults = Application::defaultCustomColorPalette();
        QStringList colors;
        for (const std::string& color : defaults.hexColors)
        {
            colors.append(QString::fromUtf8(
                color.data(),
                static_cast<qsizetype>(color.size())
                ));
        }
        return colors;
    }

    [[nodiscard]] static QStringList colorsFromJson(
        const QString& text
        )
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

    [[nodiscard]] static QStringList normalizeCustomColors(
        const QStringList& source
        )
    {
        QStringList colors = defaultCustomColors();
        for (int index = 0;
             index < std::min(
                 source.size(),
                 static_cast<qsizetype>(
                     Application::CustomColorPalette::EntryCount
                     )
                 );
             ++index)
        {
            const QColor color(source.at(index).trimmed());
            if (color.isValid())
            {
                colors[index] = color.name(QColor::HexRgb);
            }
        }
        return colors;
    }

    [[nodiscard]] static QStringList toQStringList(
        const Application::CustomColorPalette& palette
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
            palette.hexColors[index] = std::string(
                encoded.constData(),
                static_cast<std::size_t>(encoded.size())
                );
        }
        return palette;
    }

    [[nodiscard]] static QString serializeCustomColors(
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

    [[nodiscard]] static QString key()
    {
        return QStringLiteral("custom_colors");
    }

    SettingsService* m_settingsService = nullptr;
};

} // namespace ClassMngr::Next::Platform
