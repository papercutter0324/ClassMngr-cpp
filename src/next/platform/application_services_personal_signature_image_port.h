#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "features/my_info/data/signature_image_processor.h"
#include "next/application/personal_signature_image.h"

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <string>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the read-only personal signature image. The exact
// key, Base64 conversion, and one-time image preparation remain here; callers
// receive only opaque prepared bytes.
class ApplicationServicesPersonalSignatureImagePort final
    : public Application::PersonalSignatureImagePort
{
public:
    explicit ApplicationServicesPersonalSignatureImagePort(
        ApplicationServices& services
        ) noexcept
        : m_settingsService(services.settingsService())
    {
    }

    explicit ApplicationServicesPersonalSignatureImagePort(
        ApplicationServices* services
        ) noexcept
        : m_settingsService(
              services
                  ? services->settingsService()
                  : nullptr
              )
    {
    }

    ApplicationServicesPersonalSignatureImagePort(
        const ApplicationServicesPersonalSignatureImagePort&
        ) = delete;
    ApplicationServicesPersonalSignatureImagePort& operator=(
        const ApplicationServicesPersonalSignatureImagePort&
        ) = delete;
    ApplicationServicesPersonalSignatureImagePort(
        ApplicationServicesPersonalSignatureImagePort&&
        ) = delete;
    ApplicationServicesPersonalSignatureImagePort& operator=(
        ApplicationServicesPersonalSignatureImagePort&&
        ) = delete;

    [[nodiscard]] std::string read() const override
    {
        if (!m_settingsService || !m_settingsService->isAvailable())
        {
            return {};
        }

        const QByteArray encodedImage =
            QByteArray::fromBase64(
                m_settingsService
                    ->loadOrDefault(
                        key(),
                        QString()
                        )
                    .toString()
                    .toLatin1()
                );
        const QByteArray preparedImage =
            SignatureImage::prepareForEmbedding(encodedImage);

        return std::string(
            preparedImage.constData(),
            static_cast<std::size_t>(preparedImage.size())
            );
    }

private:
    [[nodiscard]] static QString key()
    {
        return QStringLiteral("myInfo/signatureImage");
    }

    SettingsService* m_settingsService = nullptr;
};

} // namespace ClassMngr::Next::Platform
