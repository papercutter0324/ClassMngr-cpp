#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/personal_display_name_preferences.h"

#include <QByteArray>
#include <QDebug>
#include <QString>
#include <QVariant>

#include <cstddef>
#include <string>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the personal display name. The exact legacy key,
// QVariant/UTF-8 conversion, and repository failure mapping stay here;
// personal-details writers remain compatibility owners.
class ApplicationServicesPersonalDisplayNamePreferencesPort final
    : public Application::PersonalDisplayNamePreferencesPort
{
public:
    explicit ApplicationServicesPersonalDisplayNamePreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_session(services.databaseSession())
    {
    }

    ApplicationServicesPersonalDisplayNamePreferencesPort(
        const ApplicationServicesPersonalDisplayNamePreferencesPort&
        ) = delete;
    ApplicationServicesPersonalDisplayNamePreferencesPort& operator=(
        const ApplicationServicesPersonalDisplayNamePreferencesPort&
        ) = delete;
    ApplicationServicesPersonalDisplayNamePreferencesPort(
        ApplicationServicesPersonalDisplayNamePreferencesPort&&
        ) = delete;
    ApplicationServicesPersonalDisplayNamePreferencesPort& operator=(
        ApplicationServicesPersonalDisplayNamePreferencesPort&&
        ) = delete;

    [[nodiscard]] std::string read() const override
    {
        if (!m_session || !m_session->isOpen())
        {
            return {};
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return {};
        }

        const auto stored = repository->loadSetting(key());
        QVariant storedName = QString();
        if (!stored)
        {
            qWarning()
                << "Failed to load setting"
                << key()
                << ':'
                << stored.error();
        }
        else if (stored->isValid())
        {
            storedName = *stored;
        }

        const QByteArray storedNameUtf8 = storedName.toString().toUtf8();

        return std::string(
            storedNameUtf8.constData(),
            static_cast<std::size_t>(storedNameUtf8.size())
            );
    }

    [[nodiscard]] Application::PersonalDisplayNamePreferencesSaveResult
    write(
        const std::string& name
        ) const override
    {
        if (!m_session || !m_session->isOpen())
        {
            return Application::
                PersonalDisplayNamePreferencesSaveResult::success();
        }

        SettingsRepository* const repository = m_session->settingsRepository();
        if (!repository)
        {
            return failure("Personal display name could not be saved.");
        }

        const Status saved = repository->saveSetting(
            key(),
            fromUtf8(name)
            );
        if (!saved)
        {
            const QByteArray errorBytes = saved.error().toUtf8();
            return failure(
                errorBytes.isEmpty()
                    ? "Personal display name could not be saved."
                    : errorBytes.toStdString()
                );
        }

        return Application::
            PersonalDisplayNamePreferencesSaveResult::success();
    }

private:
    [[nodiscard]] static Application::
    PersonalDisplayNamePreferencesSaveResult failure(std::string message)
    {
        return Application::PersonalDisplayNamePreferencesSaveResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = message,
            .recoverable = false
        });
    }

    [[nodiscard]] static QString key()
    {
        return QStringLiteral("myInfo/name");
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

    DatabaseSession* m_session = nullptr;
};

} // namespace ClassMngr::Next::Platform
