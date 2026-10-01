#pragma once

#include "core/application_services.h"
#include "next/application/sub_prep_preferences.h"

#include <QByteArray>
#include <QString>
#include <QVariant>

#include <cstddef>
#include <optional>
#include <string>

class DatabaseSession;

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the Sub Prep saved-content settings. The exact
// legacy keys, QVariant conversion, session availability, and atomic
// repository saves remain outside the application contract.
class ApplicationServicesSubPrepPreferencesPort final
    : public Application::SubPrepPreferencesPort
{
public:
    explicit ApplicationServicesSubPrepPreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_session(services.databaseSession())
    {
    }

    explicit ApplicationServicesSubPrepPreferencesPort(
        ApplicationServices* services
        ) noexcept
        : m_session(services ? services->databaseSession() : nullptr)
    {
    }

    ApplicationServicesSubPrepPreferencesPort(
        const ApplicationServicesSubPrepPreferencesPort&
        ) = delete;
    ApplicationServicesSubPrepPreferencesPort& operator=(
        const ApplicationServicesSubPrepPreferencesPort&
        ) = delete;
    ApplicationServicesSubPrepPreferencesPort(
        ApplicationServicesSubPrepPreferencesPort&&
        ) = delete;
    ApplicationServicesSubPrepPreferencesPort& operator=(
        ApplicationServicesSubPrepPreferencesPort&&
        ) = delete;

    [[nodiscard]] Application::SubPrepPreferencesResult load()
        const override;

    [[nodiscard]] Application::SubPrepPreferencesSaveResult save(
        const Application::SubPrepPreferences& preferences
        ) const override;

private:
    [[nodiscard]] static QString classMaterialsKey()
    {
        return QStringLiteral("subPrep/classMaterials");
    }

    [[nodiscard]] static QString bookReportGradingKey()
    {
        return QStringLiteral("subPrep/bookReportGrading");
    }

    [[nodiscard]] static QString bookReportSpecialInstructionsKey()
    {
        return QStringLiteral("subPrep/bookReportSpecialInstructions");
    }

    [[nodiscard]] static QString subCommentsKey()
    {
        return QStringLiteral("subPrep/subComments");
    }

    [[nodiscard]] static Domain::OperationError unavailableError()
    {
        return {
            .code = Domain::ErrorCode::Technical,
            .message = "Sub Prep settings service is unavailable.",
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

    [[nodiscard]] static QString fromUtf8(
        const std::string& value
        )
    {
        return QString::fromUtf8(
            value.data(),
            static_cast<qsizetype>(value.size())
            );
    }

    [[nodiscard]] static std::optional<std::string> optionalUtf8(
        const QVariant& value
        )
    {
        if (!value.isValid())
        {
            return std::nullopt;
        }

        return toUtf8(value.toString());
    }

    DatabaseSession* m_session = nullptr;
};

} // namespace ClassMngr::Next::Platform
