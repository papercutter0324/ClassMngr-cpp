#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/sub_prep_preferences.h"

#include <QByteArray>
#include <QString>
#include <QVariant>

#include <cstddef>
#include <optional>
#include <string>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the Sub Prep saved-content settings. The exact
// legacy keys, QVariant conversion, service availability, and atomic saveAll
// remain outside the application contract.
class ApplicationServicesSubPrepPreferencesPort final
    : public Application::SubPrepPreferencesPort
{
public:
    explicit ApplicationServicesSubPrepPreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_settingsService(services.settingsService())
    {
    }

    explicit ApplicationServicesSubPrepPreferencesPort(
        SettingsService* settingsService
        ) noexcept
        : m_settingsService(settingsService)
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
        const override
    {
        if (!m_settingsService || !m_settingsService->isAvailable())
        {
            return Application::SubPrepPreferencesResult::failure(
                unavailableError()
                );
        }

        return Application::SubPrepPreferencesResult::success({
            .classMaterials =
                toUtf8(
                    m_settingsService->loadOrDefault(
                        classMaterialsKey(),
                        QString()
                        ).toString()
                    ),
            .bookReportGrading =
                optionalUtf8(
                    m_settingsService->loadOrDefault(
                        bookReportGradingKey(),
                        QVariant()
                        )
                    ),
            .bookReportSpecialInstructions =
                optionalUtf8(
                    m_settingsService->loadOrDefault(
                        bookReportSpecialInstructionsKey(),
                        QVariant()
                        )
                    ),
            .subComments =
                toUtf8(
                    m_settingsService->loadOrDefault(
                        subCommentsKey(),
                        QString()
                        ).toString()
                    )
        });
    }

    [[nodiscard]] Application::SubPrepPreferencesSaveResult save(
        const Application::SubPrepPreferences& preferences
        ) const override
    {
        if (!m_settingsService || !m_settingsService->isAvailable())
        {
            return Application::SubPrepPreferencesSaveResult::failure(
                unavailableError()
                );
        }

        const Status saved = m_settingsService->saveAll({
            {
                classMaterialsKey(),
                fromUtf8(preferences.classMaterials)
            },
            {
                bookReportGradingKey(),
                preferences.bookReportGrading
                    ? QVariant(fromUtf8(*preferences.bookReportGrading))
                    : QVariant()
            },
            {
                bookReportSpecialInstructionsKey(),
                preferences.bookReportSpecialInstructions
                    ? QVariant(
                        fromUtf8(
                            *preferences.bookReportSpecialInstructions
                            )
                        )
                    : QVariant()
            },
            {
                subCommentsKey(),
                fromUtf8(preferences.subComments)
            }
        });
        if (!saved)
        {
            const QByteArray errorBytes = saved.error().toUtf8();
            return Application::SubPrepPreferencesSaveResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = errorBytes.isEmpty()
                    ? "Sub Prep preferences could not be saved."
                    : errorBytes.toStdString(),
                .recoverable = false
            });
        }

        return Application::SubPrepPreferencesSaveResult::success();
    }

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

    SettingsService* m_settingsService = nullptr;
};

} // namespace ClassMngr::Next::Platform
