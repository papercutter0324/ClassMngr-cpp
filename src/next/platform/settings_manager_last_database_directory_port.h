#pragma once

#include "core/settingsmanager.h"
#include "next/application/last_database_directory_port.h"

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the last database directory. The legacy
// SettingsManager singleton and checked preference-specific writer remain
// confined here; callers consume the typed UTF-8 read/write boundary.
class SettingsManagerLastDatabaseDirectoryPort final
    : public Application::LastDatabaseDirectoryPort
{
public:
    explicit SettingsManagerLastDatabaseDirectoryPort(
        SettingsManager::LastDatabaseDirectoryPreferenceSync synchronize = {}
        )
        : m_synchronize(std::move(synchronize))
    {
    }

    [[nodiscard]] Application::DirectoryPreferencePath read() const override
    {
        const QString storedDirectory =
            SettingsManager::instance().get(
                key(),
                QString()
                ).toString();

        return Application::DirectoryPreferencePath(
            toUtf8(storedDirectory)
            );
    }

    [[nodiscard]] Domain::Result<void> write(
        const Application::DirectoryPreferencePath& directory
        ) const override
    {
        const QSettings::Status status =
            SettingsManager::instance().writeLastDatabaseDirectoryPreference(
                fromUtf8(directory.value()),
                m_synchronize
                );
        if (status == QSettings::NoError)
        {
            return Domain::Result<void>::success();
        }

        const char* statusName = status == QSettings::AccessError
            ? "AccessError (settings could not be written)"
            : "FormatError (settings contain invalid data)";
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = std::string(
                "The last database directory preference could not be saved: "
                "QSettings "
                ) + statusName + ".",
            .recoverable = true
        });
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

    SettingsManager::LastDatabaseDirectoryPreferenceSync m_synchronize;
};

} // namespace ClassMngr::Next::Platform
