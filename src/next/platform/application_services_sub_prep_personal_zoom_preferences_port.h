#pragma once

#include "data/database/workspace_store_fwd.h"

#include "core/application_services.h"
#include "next/application/sub_prep_personal_zoom_preferences.h"

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <string>


namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for Sub Prep personal-Zoom settings. Primary values
// win; legacy values are read only when primary values are missing or invalid.
// Legacy values migrate best-effort after all required settings were read,
// without changing the typed result if a migration save fails.
class ApplicationServicesSubPrepPersonalZoomPreferencesPort final
    : public Application::SubPrepPersonalZoomPreferencesPort
{
public:
    explicit ApplicationServicesSubPrepPersonalZoomPreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_store(services.workspaceStore())
    {
    }

    explicit ApplicationServicesSubPrepPersonalZoomPreferencesPort(
        ApplicationServices* services
        ) noexcept
        : m_store(services ? services->workspaceStore() : nullptr)
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
        const override;

private:
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

    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform
