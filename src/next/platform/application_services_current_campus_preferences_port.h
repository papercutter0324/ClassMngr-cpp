#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/current_campus_preferences.h"

#include <QByteArray>
#include <QDebug>
#include <QString>
#include <QVariant>

#include <cstddef>
#include <string>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the current-campus preference. The exact legacy key,
// QVariant/UTF-8 conversion, and repository failure mapping stay here;
// PersonalDetails remains the aggregate compatibility writer.
class ApplicationServicesCurrentCampusPreferencesPort final
    : public Application::CurrentCampusPreferencesPort
{
public:
    explicit ApplicationServicesCurrentCampusPreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_session(services.databaseSession())
    {
    }

    explicit ApplicationServicesCurrentCampusPreferencesPort(
        ApplicationServices* services
        ) noexcept
        : m_session(services ? services->databaseSession() : nullptr)
    {
    }

    ApplicationServicesCurrentCampusPreferencesPort(
        const ApplicationServicesCurrentCampusPreferencesPort&
        ) = delete;
    ApplicationServicesCurrentCampusPreferencesPort& operator=(
        const ApplicationServicesCurrentCampusPreferencesPort&
        ) = delete;
    ApplicationServicesCurrentCampusPreferencesPort(
        ApplicationServicesCurrentCampusPreferencesPort&&
        ) = delete;
    ApplicationServicesCurrentCampusPreferencesPort& operator=(
        ApplicationServicesCurrentCampusPreferencesPort&&
        ) = delete;

    [[nodiscard]] bool isAvailable() const override
    {
        return m_session && m_session->isOpen();
    }

    [[nodiscard]] Domain::Result<std::string> read() const override
    {
        if (!isAvailable())
        {
            return Domain::Result<std::string>::success({});
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return Domain::Result<std::string>::failure({
                .code = Domain::ErrorCode::Technical,
                .message =
                    "The settings repository is unavailable while reading "
                    "the current campus preference.",
                .recoverable = true
            });
        }

        const auto stored = repository->loadSetting(key());
        if (!stored)
        {
            qWarning()
                << "Failed to load setting"
                << key()
                << ':'
                << stored.error();

            const QByteArray errorBytes = stored.error().toUtf8();
            return Domain::Result<std::string>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = errorBytes.isEmpty()
                    ? "The current campus preference could not be read."
                    : errorBytes.toStdString(),
                .recoverable = true
            });
        }

        const QVariant campus = stored->isValid()
            ? *stored
            : QVariant(QString());

        const QByteArray storedCampus = campus.toString().toUtf8();

        return Domain::Result<std::string>::success(
            std::string(
                storedCampus.constData(),
                static_cast<std::size_t>(storedCampus.size())
                )
            );
    }

    [[nodiscard]] Domain::Result<void> write(
        const std::string& campus
        ) const override
    {
        if (!isAvailable())
        {
            return Domain::Result<void>::success();
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "Current campus could not be saved.",
                .recoverable = false
            });
        }

        const Status saved = repository->saveSetting(
            key(),
            QString::fromUtf8(
                campus.data(),
                static_cast<qsizetype>(campus.size())
                )
            );
        if (!saved)
        {
            const QByteArray errorBytes = saved.error().toUtf8();
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = errorBytes.isEmpty()
                    ? "Current campus could not be saved."
                    : errorBytes.toStdString(),
                .recoverable = false
            });
        }

        return Domain::Result<void>::success();
    }

private:
    [[nodiscard]] static QString key()
    {
        return QStringLiteral("myInfo/campus");
    }

    DatabaseSession* m_session = nullptr;
};

} // namespace ClassMngr::Next::Platform
