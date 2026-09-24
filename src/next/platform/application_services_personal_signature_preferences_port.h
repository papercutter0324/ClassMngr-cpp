#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/personal_signature_preferences.h"

#include <QByteArray>
#include <QString>
#include <QVariant>

#include <cstddef>
#include <string>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the read-only personal signature preferences. Exact
// keys, QVariant conversion, and persisted-mode normalization remain here;
// the caller receives only typed values and opaque UTF-8 text.
class ApplicationServicesPersonalSignaturePreferencesPort final
    : public Application::PersonalSignaturePreferencesPort
{
public:
    explicit ApplicationServicesPersonalSignaturePreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesPersonalSignaturePreferencesPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
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
        SettingsService* const settingsService =
            m_services
                ? m_services->settingsService()
                : nullptr;
        if (!settingsService || !settingsService->isAvailable())
        {
            return Application::
                PersonalSignaturePreferencesResult::failure(
                    unavailableError()
                    );
        }

        const int storedMode =
            settingsService
                ->loadOrDefault(
                    signatureModeKey(),
                    0
                    )
                .toInt();
        const int storedFont =
            settingsService
                ->loadOrDefault(
                    typedSignatureFontKey(),
                    0
                    )
                .toInt();
        const QByteArray storedText =
            settingsService
                ->loadOrDefault(
                    typedSignatureTextKey(),
                    QString()
                    )
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

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
