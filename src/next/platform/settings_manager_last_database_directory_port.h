#pragma once

#include "core/settingsmanager.h"
#include "next/application/last_database_directory_port.h"

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <string>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the last database directory. The legacy
// SettingsManager singleton and its synchronized setter remain confined here;
// callers consume only the typed UTF-8 string boundary.
class SettingsManagerLastDatabaseDirectoryPort final
    : public Application::LastDatabaseDirectoryPort
{
public:
    SettingsManagerLastDatabaseDirectoryPort() = default;

    [[nodiscard]] std::string read() const override
    {
        const QString storedDirectory =
            SettingsManager::instance().get(
                key(),
                QString()
                ).toString();

        return toUtf8(storedDirectory);
    }

    void write(
        const std::string& directory
        ) const override
    {
        // Use the legacy setter so its existing QSettings synchronization
        // behavior remains unchanged.
        SettingsManager::instance().setLastDatabaseDirectory(
            fromUtf8(directory)
            );
    }

private:
    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            SettingsManager::Keys::LAST_DATABASE_DIRECTORY
            );
    }

    [[nodiscard]] static std::string toUtf8(
        const QString& value
        )
    {
        const QByteArray encoded = value.toUtf8();
        return std::string(
            encoded.constData(),
            static_cast<std::size_t>(encoded.size())
            );
    }

    [[nodiscard]] static QString fromUtf8(
        const std::string& value
        )
    {
        return QString::fromUtf8(
            value.data(),
            static_cast<qsizetype>(value.size())
            );
    }
};

} // namespace ClassMngr::Next::Platform
