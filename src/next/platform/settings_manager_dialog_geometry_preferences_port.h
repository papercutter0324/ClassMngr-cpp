#pragma once

#include "core/settingsmanager.h"
#include "next/application/dialog_geometry_preferences_port.h"

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <string>
#include <string_view>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for DialogShell geometry. The SettingsManager singleton,
// normalized dynamic key, and binary QVariant conversion stay confined here.
class SettingsManagerDialogGeometryPreferencesPort final
    : public Application::DialogGeometryPreferencesPort
{
public:
    SettingsManagerDialogGeometryPreferencesPort() = default;

    [[nodiscard]] std::string read(
        std::string_view stableDialogKey
        ) const override
    {
        const QString normalizedKey = normalizedDialogKey(stableDialogKey);
        if (normalizedKey.isEmpty())
        {
            return {};
        }

        const QByteArray geometry = SettingsManager::instance().get(
            settingsKey(normalizedKey)
            ).toByteArray();

        return toBinaryString(geometry);
    }

    void write(
        std::string_view stableDialogKey,
        std::string_view geometryPayload
        ) const override
    {
        const QString normalizedKey = normalizedDialogKey(stableDialogKey);
        if (normalizedKey.isEmpty())
        {
            return;
        }

        SettingsManager::instance().set(
            settingsKey(normalizedKey),
            QByteArray(
                geometryPayload.data(),
                static_cast<qsizetype>(geometryPayload.size())
                )
            );
    }

private:
    [[nodiscard]] static QString normalizedDialogKey(
        std::string_view stableDialogKey
        )
    {
        QString key = QString::fromUtf8(
            stableDialogKey.data(),
            static_cast<qsizetype>(stableDialogKey.size())
            ).trimmed();

        for (QChar& character : key)
        {
            if (
                !character.isLetterOrNumber()
                && character != u'-'
                && character != u'_'
                && character != u'.'
                )
            {
                character = u'_';
            }
        }

        return key;
    }

    [[nodiscard]] static QString settingsKey(
        const QString& normalizedKey
        )
    {
        return QStringLiteral("ui/dialogs/%1/geometry")
            .arg(normalizedKey);
    }

    [[nodiscard]] static std::string toBinaryString(
        const QByteArray& value
        )
    {
        return std::string(
            value.constData(),
            static_cast<std::size_t>(value.size())
            );
    }
};

} // namespace ClassMngr::Next::Platform
