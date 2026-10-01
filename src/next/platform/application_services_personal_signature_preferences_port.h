#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/personal_signature_preferences.h"

#include <QByteArray>
#include <QDebug>
#include <QString>
#include <QVariant>

#include <cstddef>
#include <string>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the read-only personal signature preferences. Exact
// keys, QVariant conversion, and persisted-mode normalization remain here;
// the caller receives only typed values and opaque UTF-8 text. Reads use only
// the active session repository and never materialize missing defaults.
class ApplicationServicesPersonalSignaturePreferencesPort final
    : public Application::PersonalSignaturePreferencesPort
{
public:
    explicit ApplicationServicesPersonalSignaturePreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_session(services.databaseSession())
    {
    }

    explicit ApplicationServicesPersonalSignaturePreferencesPort(
        ApplicationServices* services
        ) noexcept
        : m_session(services ? services->databaseSession() : nullptr)
    {
    }

    ApplicationServicesPersonalSignaturePreferencesPort(
        const ApplicationServicesPersonalSignaturePreferencesPort&
        ) = delete;
    ApplicationServicesPersonalSignaturePreferencesPort& operator=(
        const ApplicationServicesPersonalSignaturePreferencesPort&
        ) = delete;
    ApplicationServicesPersonalSignaturePreferencesPort(
        ApplicationServicesPersonalSignaturePreferencesPort&&
        ) = delete;
    ApplicationServicesPersonalSignaturePreferencesPort& operator=(
        ApplicationServicesPersonalSignaturePreferencesPort&&
        ) = delete;

    [[nodiscard]] Application::PersonalSignaturePreferencesResult load()
        const override
    {
        if (!m_session || !m_session->isOpen())
        {
            return Application::
                PersonalSignaturePreferencesResult::failure(
                    unavailableError()
                    );
        }

        SettingsRepository* const repository =
            m_session->settingsRepository();
        if (!repository)
        {
            return Application::
                PersonalSignaturePreferencesResult::failure(
                    unavailableError()
                    );
        }

        const int storedMode =
            loadOrDefault(*repository, signatureModeKey(), 0).toInt();
        const int storedFont =
            loadOrDefault(*repository, typedSignatureFontKey(), 0).toInt();
        const QByteArray storedText =
            loadOrDefault(*repository, typedSignatureTextKey(), QString())
                .toString()
                .toUtf8();

        return Application::PersonalSignaturePreferencesResult::success({
            .mode = storedMode == 1
                ? Application::PersonalSignatureMode::Type
                : Application::PersonalSignatureMode::Image,
            .typedSignatureText = std::string(
                storedText.constData(),
                static_cast<std::size_t>(storedText.size())
                ),
            .typedSignatureFont = storedFont
        });
    }

private:
    [[nodiscard]] static QVariant loadOrDefault(
        SettingsRepository& repository,
        const QString& key,
        const QVariant& defaultValue
        )
    {
        const Result<QVariant> stored = repository.loadSetting(key);
        if (!stored)
        {
            qWarning()
                << "Failed to load setting"
                << key
                << ':'
                << stored.error();
            return defaultValue;
        }

        return stored->isValid()
            ? *stored
            : defaultValue;
    }

    [[nodiscard]] static QString signatureModeKey()
    {
        return QStringLiteral("myInfo/signatureMode");
    }

    [[nodiscard]] static QString typedSignatureTextKey()
    {
        return QStringLiteral("myInfo/typedSignatureText");
    }

    [[nodiscard]] static QString typedSignatureFontKey()
    {
        return QStringLiteral("myInfo/typedSignatureFont");
    }

    [[nodiscard]] static Domain::OperationError unavailableError()
    {
        return {
            .code = Domain::ErrorCode::Technical,
            .message = "Personal signature preferences service is unavailable.",
            .recoverable = false
        };
    }

    DatabaseSession* m_session = nullptr;
};

} // namespace ClassMngr::Next::Platform
