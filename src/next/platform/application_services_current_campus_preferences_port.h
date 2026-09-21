#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/current_campus_preferences.h"

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <string>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the current-campus preference. The exact legacy key,
// QVariant/UTF-8 conversion, and SettingsService failure mapping stay here;
// PersonalDetails remains the aggregate compatibility writer.
class ApplicationServicesCurrentCampusPreferencesPort final
    : public Application::CurrentCampusPreferencesPort
{
public:
    explicit ApplicationServicesCurrentCampusPreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_settingsService(services.settingsService())
    {
    }

    explicit ApplicationServicesCurrentCampusPreferencesPort(
        SettingsService* settingsService
        ) noexcept
        : m_settingsService(settingsService)
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

    [[nodiscard]] std::string read() const override
    {
        if (!m_settingsService || !m_settingsService->isAvailable())
        {
            return {};
        }

        const QByteArray storedCampus =
            m_settingsService
                ->loadOrDefault(
                    key(),
                    QString()
                    )
                .toString()
                .toUtf8();

        return std::string(
            storedCampus.constData(),
            static_cast<std::size_t>(storedCampus.size())
            );
    }

    [[nodiscard]] Domain::Result<void> write(
        const std::string& campus
        ) const override
    {
        if (!m_settingsService || !m_settingsService->isAvailable())
        {
            return Domain::Result<void>::success();
        }

        const Status saved = m_settingsService->save(
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

    SettingsService* m_settingsService = nullptr;
};

} // namespace ClassMngr::Next::Platform
