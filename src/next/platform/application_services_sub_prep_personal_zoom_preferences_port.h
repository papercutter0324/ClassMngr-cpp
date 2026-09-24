#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/sub_prep_personal_zoom_preferences.h"

#include <QByteArray>
#include <QString>
#include <QVariant>

#include <cstddef>
#include <string>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for Sub Prep personal-Zoom settings. Primary values
// win; legacy values are read only when primary values are absent and are
// migrated best-effort without changing the typed read result on save error.
class ApplicationServicesSubPrepPersonalZoomPreferencesPort final
    : public Application::SubPrepPersonalZoomPreferencesPort
{
public:
    explicit ApplicationServicesSubPrepPersonalZoomPreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_settingsService(services.settingsService())
    {
    }

    explicit ApplicationServicesSubPrepPersonalZoomPreferencesPort(
        ApplicationServices* services
        ) noexcept
        : m_settingsService(
            services ? services->settingsService() : nullptr
            )
    {
    }

    ApplicationServicesSubPrepPersonalZoomPreferencesPort(
        const ApplicationServicesSubPrepPersonalZoomPreferencesPort&
        ) = delete;
    ApplicationServicesSubPrepPersonalZoomPreferencesPort& operator=(
        const ApplicationServicesSubPrepPersonalZoomPreferencesPort&
        ) = delete;
    ApplicationServicesSubPrepPersonalZoomPreferencesPort(
        ApplicationServicesSubPrepPersonalZoomPreferencesPort&&
        ) = delete;
    ApplicationServicesSubPrepPersonalZoomPreferencesPort& operator=(
        ApplicationServicesSubPrepPersonalZoomPreferencesPort&&
        ) = delete;

    [[nodiscard]] Application::SubPrepPersonalZoomPreferencesResult load()
        const override
    {
        if (!m_settingsService || !m_settingsService->isAvailable())
        {
            return Application::
                SubPrepPersonalZoomPreferencesResult::failure(
                    unavailableError()
                    );
        }

        const StoredValue loginId =
            readWithLegacyFallback(
                primaryLoginIdKey(),
                legacyLoginIdKey()
                );
        const StoredValue password =
            readWithLegacyFallback(
                primaryPasswordKey(),
                legacyPasswordKey()
                );
        const StoredValue unavailable =
            readWithLegacyFallback(
                primaryUnavailableKey(),
                legacyUnavailableKey()
                );

        return Application::
            SubPrepPersonalZoomPreferencesResult::success({
                .loginId = loginId.value.isValid()
                    ? toUtf8(loginId.value.toString())
                    : std::string("N/A"),
                .password = password.value.isValid()
                    ? toUtf8(password.value.toString())
                    : std::string("N/A"),
                .unavailable = unavailable.value.isValid()
                    ? unavailable.value.toBool()
                    : true
            });
    }

private:
    struct StoredValue final
    {
        QVariant value;
    };

    [[nodiscard]] static QString primaryLoginIdKey()
    {
        return QStringLiteral("myInfo/zoomLoginId");
    }

    [[nodiscard]] static QString primaryPasswordKey()
    {
        return QStringLiteral("myInfo/zoomPassword");
    }

    [[nodiscard]] static QString primaryUnavailableKey()
    {
        return QStringLiteral("myInfo/zoomNotAvailable");
    }

    [[nodiscard]] static QString legacyLoginIdKey()
    {
        return QStringLiteral("subPrep/personalZoomEmail");
    }

    [[nodiscard]] static QString legacyPasswordKey()
    {
        return QStringLiteral("subPrep/personalZoomPassword");
    }

    [[nodiscard]] static QString legacyUnavailableKey()
    {
        return QStringLiteral("subPrep/personalZoomNotAvailable");
    }

    [[nodiscard]] static Domain::OperationError unavailableError()
    {
        return {
            .code = Domain::ErrorCode::Technical,
            .message = "Sub Prep personal Zoom settings service is unavailable.",
            .recoverable = false
        };
    }

    [[nodiscard]] StoredValue readWithLegacyFallback(
        const QString& primaryKey,
        const QString& legacyKey
        ) const
    {
        const QVariant primaryValue =
            m_settingsService->loadOrDefault(
                primaryKey,
                QVariant()
                );
        if (primaryValue.isValid())
        {
            return {primaryValue};
        }

        const QVariant legacyValue =
            m_settingsService->loadOrDefault(
                legacyKey,
                QVariant()
                );
        if (!legacyValue.isValid())
        {
            return {QVariant()};
        }

        // Migration is intentionally best-effort, matching the previous
        // helper: the legacy value remains the successful read result.
        static_cast<void>(
            m_settingsService->save(
                primaryKey,
                legacyValue
                )
            );

        return {legacyValue};
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

    SettingsService* m_settingsService = nullptr;
};

} // namespace ClassMngr::Next::Platform
