#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "features/my_info/data/signature_image_processor.h"
#include "next/application/personal_details_save.h"

#include <QByteArray>
#include <QString>
#include <QVariantMap>

#include <cstddef>
#include <string>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the atomic Personal Details compatibility bundle.
// It owns the nine exact keys, QVariant conversion, image preparation, and
// the single SettingsService::saveAll transaction.
class ApplicationServicesPersonalDetailsSavePort final
    : public Application::PersonalDetailsSavePort
{
public:
    explicit ApplicationServicesPersonalDetailsSavePort(
        ApplicationServices& services
        ) noexcept
        : m_settingsService(services.settingsService())
    {
    }

    explicit ApplicationServicesPersonalDetailsSavePort(
        SettingsService* settingsService
        ) noexcept
        : m_settingsService(settingsService)
    {
    }

    ApplicationServicesPersonalDetailsSavePort(
        const ApplicationServicesPersonalDetailsSavePort&
        ) = delete;
    ApplicationServicesPersonalDetailsSavePort& operator=(
        const ApplicationServicesPersonalDetailsSavePort&
        ) = delete;
    ApplicationServicesPersonalDetailsSavePort(
        ApplicationServicesPersonalDetailsSavePort&&
        ) = delete;
    ApplicationServicesPersonalDetailsSavePort& operator=(
        ApplicationServicesPersonalDetailsSavePort&&
        ) = delete;

    [[nodiscard]] Application::PersonalDetailsSaveResult save(
        const Application::PersonalDetailsSaveRequest& request
        ) const override
    {
        if (!m_settingsService || !m_settingsService->isAvailable())
        {
            return Application::PersonalDetailsSaveResult::failure(
                unavailableError()
                );
        }

        const QVariantMap values = {
            {
                nameKey(),
                fromUtf8(request.name)
            },
            {
                campusKey(),
                fromUtf8(request.campus)
            },
            {
                zoomLoginIdKey(),
                fromUtf8(request.zoomLoginId)
            },
            {
                zoomPasswordKey(),
                fromUtf8(request.zoomPassword)
            },
            {
                zoomNotAvailableKey(),
                request.zoomNotAvailable
            },
            {
                signatureImageKey(),
                encodedSignatureImage(request.signatureImage)
            },
            {
                signatureModeKey(),
                normalizedSignatureMode(request.signatureMode)
            },
            {
                typedSignatureTextKey(),
                fromUtf8(request.typedSignatureText)
            },
            {
                typedSignatureFontKey(),
                normalizedSignatureFont(request.typedSignatureFont)
            }
        };

        const Status saved = m_settingsService->saveAll(values);
        if (!saved)
        {
            const QByteArray errorBytes = saved.error().toUtf8();
            return Application::PersonalDetailsSaveResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = errorBytes.isEmpty()
                    ? "Personal details could not be saved."
                    : errorBytes.toStdString(),
                .recoverable = false
            });
        }

        return Application::PersonalDetailsSaveResult::success();
    }

private:
    [[nodiscard]] static QString nameKey()
    {
        return QStringLiteral("myInfo/name");
    }

    [[nodiscard]] static QString campusKey()
    {
        return QStringLiteral("myInfo/campus");
    }

    [[nodiscard]] static QString zoomLoginIdKey()
    {
        return QStringLiteral("myInfo/zoomLoginId");
    }

    [[nodiscard]] static QString zoomPasswordKey()
    {
        return QStringLiteral("myInfo/zoomPassword");
    }

    [[nodiscard]] static QString zoomNotAvailableKey()
    {
        return QStringLiteral("myInfo/zoomNotAvailable");
    }

    [[nodiscard]] static QString signatureImageKey()
    {
        return QStringLiteral("myInfo/signatureImage");
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
            .message = "Personal details settings service is unavailable.",
            .recoverable = false
        };
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

    [[nodiscard]] static QString encodedSignatureImage(
        const std::string& image
        )
    {
        if (image.empty())
        {
            return {};
        }

        const QByteArray prepared =
            SignatureImage::prepareForEmbedding(
                QByteArray(
                    image.data(),
                    static_cast<qsizetype>(image.size())
                    )
                );
        return QString::fromLatin1(prepared.toBase64());
    }

    [[nodiscard]] static int normalizedSignatureMode(
        Application::PersonalSignatureMode mode
        )
    {
        return mode == Application::PersonalSignatureMode::Type
            ? 1
            : 0;
    }

    [[nodiscard]] static int normalizedSignatureFont(int font)
    {
        return font >= 0 && font <= 3
            ? font
            : 0;
    }

    SettingsService* m_settingsService = nullptr;
};

} // namespace ClassMngr::Next::Platform
