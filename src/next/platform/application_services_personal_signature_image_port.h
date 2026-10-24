#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/settings_repository.h"
#include "features/my_info/data/signature_image_processor.h"
#include "next/application/personal_signature_image.h"

#include <QByteArray>
#include <QDebug>
#include <QString>
#include <QVariant>

#include <cstddef>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the read-only personal signature image. The exact
// key, Base64 conversion, and one-time image preparation remain here; callers
// receive only opaque prepared bytes. Reads use only the active workspace
// repository.
class ApplicationServicesPersonalSignatureImagePort final
    : public Application::PersonalSignatureImagePort
{
public:
    explicit ApplicationServicesPersonalSignatureImagePort(
        ApplicationServices& services
        ) noexcept
        : m_store(services.workspaceStore())
    {
    }

    explicit ApplicationServicesPersonalSignatureImagePort(
        ApplicationServices* services
        ) noexcept
        : m_store(services ? services->workspaceStore() : nullptr)
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

    [[nodiscard]] Application::PersonalSignatureImageResult read()
        const override
    {
        if (!m_store || !m_store->isOpen())
        {
            return Application::PersonalSignatureImageResult::success(
                Domain::PreparedPersonalSignatureImageBytes{}
                );
        }

        SettingsRepository* const repository =
            m_store->settingsRepository();
        if (!repository)
        {
            return Application::PersonalSignatureImageResult::success(
                Domain::PreparedPersonalSignatureImageBytes{}
                );
        }

        const auto stored = repository->loadSetting(key());
        QVariant encodedValue = QString();
        if (!stored)
        {
            return Application::PersonalSignatureImageResult::failure(
                settingLoadError(key(), stored.error())
                );
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

        if (preparedImage.isEmpty())
        {
            return Application::PersonalSignatureImageResult::success(
                Domain::PreparedPersonalSignatureImageBytes{}
                );
        }

        return Application::PersonalSignatureImageResult::success(
            Domain::PreparedPersonalSignatureImageBytes(
                std::string(
                    preparedImage.constData(),
                    static_cast<std::size_t>(preparedImage.size())
                    )
                )
            );
    }

private:
    [[nodiscard]] static Domain::OperationError settingLoadError(
        const QString& settingKey,
        const QString& diagnostic
        )
    {
        qWarning()
            << "Failed to load setting"
            << settingKey
            << ':'
            << diagnostic;

        std::string message = "Personal signature image could not be loaded";
        if (!diagnostic.isEmpty())
        {
            message.append(": ");
            const QByteArray diagnosticBytes = diagnostic.toUtf8();
            message.append(
                diagnosticBytes.constData(),
                static_cast<std::size_t>(diagnosticBytes.size())
                );
        }
        message.push_back('.');

        return {
            .code = Domain::ErrorCode::Technical,
            .message = std::move(message),
            .recoverable = false
        };
    }

    [[nodiscard]] static QString key()
    {
        return QStringLiteral("myInfo/signatureImage");
    }

    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform
