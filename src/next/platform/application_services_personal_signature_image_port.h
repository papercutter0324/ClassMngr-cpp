#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "features/my_info/data/signature_image_processor.h"
#include "next/application/personal_signature_image.h"

#include <QByteArray>
#include <QDebug>
#include <QString>
#include <QVariant>

#include <cstddef>
#include <string>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the read-only personal signature image. The exact
// key, Base64 conversion, and one-time image preparation remain here; callers
// receive only opaque prepared bytes. Reads use only the active session
// repository.
class ApplicationServicesPersonalSignatureImagePort final
    : public Application::PersonalSignatureImagePort
{
public:
    explicit ApplicationServicesPersonalSignatureImagePort(
        ApplicationServices& services
        ) noexcept
        : m_session(services.databaseSession())
    {
    }

    explicit ApplicationServicesPersonalSignatureImagePort(
        ApplicationServices* services
        ) noexcept
        : m_session(services ? services->databaseSession() : nullptr)
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
        if (!m_session || !m_session->isOpen())
        {
            return {};
        }

        SettingsRepository* const repository =
            m_session->settingsRepository();
        if (!repository)
        {
            return {};
        }

        const auto stored = repository->loadSetting(key());
        QVariant encodedValue = QString();
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
            encodedValue = *stored;
        }

        const QByteArray encodedImage =
            QByteArray::fromBase64(
                encodedValue.toString().toLatin1()
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

    DatabaseSession* m_session = nullptr;
};

} // namespace ClassMngr::Next::Platform
