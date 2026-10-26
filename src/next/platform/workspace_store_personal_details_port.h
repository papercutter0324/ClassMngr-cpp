#pragma once

#include "data/database/workspace_store.h"
#include "data/repositories/settings_repository.h"
#include "features/my_info/data/signature_image_processor.h"
#include "next/application/personal_details_save.h"
#include "next/platform/repository_error_code.h"

#include <QByteArray>
#include <QString>
#include <QVariant>
#include <QVariantMap>

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// The Next setup path owns the exact personal-details keys and image
// conversion at the WorkspaceStore boundary. Read returns the entire bundle
// so a failed setting read cannot be confused with an empty value. Save uses
// SettingsRepository's single transaction for all nine compatibility keys.
class WorkspaceStorePersonalDetailsPort final
    : public Application::PersonalDetailsReadPort,
      public Application::PersonalDetailsSavePort
{
public:
    explicit WorkspaceStorePersonalDetailsPort(
        WorkspaceStore& store
        ) noexcept
        : m_store(&store)
    {
    }

    WorkspaceStorePersonalDetailsPort(
        const WorkspaceStorePersonalDetailsPort& 
        ) = delete;
    WorkspaceStorePersonalDetailsPort& operator=(
        const WorkspaceStorePersonalDetailsPort&
        ) = delete;
    WorkspaceStorePersonalDetailsPort(
        WorkspaceStorePersonalDetailsPort&&
        ) = delete;
    WorkspaceStorePersonalDetailsPort& operator=(
        WorkspaceStorePersonalDetailsPort&&
        ) = delete;

    [[nodiscard]] Application::PersonalDetailsReadResult read()
        const override
    {
        if (!m_store || !m_store->isOpen())
        {
            return Application::PersonalDetailsReadResult::failure(
                unavailableError()
                );
        }

        SettingsRepository* const repository = m_store->settingsRepository();
        if (!repository)
        {
            return Application::PersonalDetailsReadResult::failure(
                unavailableError()
                );
        }

        std::optional<Domain::OperationError> readError;
        const auto load = [repository, &readError](
                              const QString& key,
                              const QVariant& defaultValue = QVariant{}
                              ) {
            const auto stored = repository->loadSetting(key);
            if (!stored)
            {
                readError = repositoryError(
                    QStringLiteral("Personal details could not be loaded"),
                    stored.error()
                    );
                return defaultValue;
            }
            return stored->isValid() ? *stored : defaultValue;
        };

        const QVariant name = load(nameKey(), QString());
        const QVariant campus = load(campusKey(), QString());

        const QVariant zoomLoginId = loadWithLegacyFallback(
            load,
            zoomLoginIdKey(),
            legacyZoomLoginIdKey(),
            QStringLiteral("N/A")
            );
        const QVariant zoomPassword = loadWithLegacyFallback(
            load,
            zoomPasswordKey(),
            legacyZoomPasswordKey(),
            QStringLiteral("N/A")
            );
        const QVariant zoomNotAvailable = loadWithLegacyFallback(
            load,
            zoomNotAvailableKey(),
            legacyZoomNotAvailableKey(),
            true
            );

        const QVariant encodedImage = load(signatureImageKey(), QString());
        const QVariant signatureMode = load(signatureModeKey(), 0);
        const QVariant typedSignatureText = load(
            typedSignatureTextKey(),
            QString()
            );
        const QVariant typedSignatureFont = load(typedSignatureFontKey(), 0);

        if (readError)
        {
            return Application::PersonalDetailsReadResult::failure(
                std::move(*readError)
                );
        }

        // Preserve the legacy Zoom fallback behavior. Migration is best
        // effort and happens only after the entire details bundle was read.
        migrateLegacyZoomSetting(
            repository,
            zoomLoginIdKey(),
            legacyZoomLoginIdKey()
            );
        migrateLegacyZoomSetting(
            repository,
            zoomPasswordKey(),
            legacyZoomPasswordKey()
            );
        migrateLegacyZoomSetting(
            repository,
            zoomNotAvailableKey(),
            legacyZoomNotAvailableKey()
            );

        const QByteArray preparedImage = SignatureImage::prepareForEmbedding(
            QByteArray::fromBase64(encodedImage.toString().toLatin1())
            );
        const QByteArray nameBytes = name.toString().toUtf8();
        const QByteArray campusBytes = campus.toString().toUtf8();
        const QByteArray zoomLoginIdBytes = zoomLoginId.toString().toUtf8();
        const QByteArray zoomPasswordBytes = zoomPassword.toString().toUtf8();
        const QByteArray typedTextBytes = typedSignatureText.toString().toUtf8();

        return Application::PersonalDetailsReadResult::success({
            .name = utf8(nameBytes),
            .campus = Application::CurrentCampusPreferenceValue(
                utf8(campusBytes)
                ),
            .zoomLoginId = utf8(zoomLoginIdBytes),
            .zoomPassword = utf8(zoomPasswordBytes),
            .zoomNotAvailable = zoomNotAvailable.toBool(),
            .signatureImage = Domain::PersonalSignatureImageSaveBytes(
                bytes(preparedImage)
                ),
            .signatureMode = signatureMode.toInt() == 1
                ? Application::PersonalSignatureMode::Type
                : Application::PersonalSignatureMode::Image,
            .typedSignatureText = utf8(typedTextBytes),
            .typedSignatureFont = signatureFontChoice(typedSignatureFont)
        });
    }

    [[nodiscard]] Application::PersonalDetailsSaveResult save(
        const Application::PersonalDetailsSaveRequest& request
        ) const override
    {
        if (!m_store || !m_store->isOpen())
        {
            return Application::PersonalDetailsSaveResult::failure(
                unavailableError()
                );
        }

        SettingsRepository* const repository = m_store->settingsRepository();
        if (!repository)
        {
            return Application::PersonalDetailsSaveResult::failure(
                unavailableError()
                );
        }

        const QVariantMap values = {
            {nameKey(), fromUtf8(request.name)},
            {campusKey(), fromUtf8(request.campus.value())},
            {zoomLoginIdKey(), fromUtf8(request.zoomLoginId)},
            {zoomPasswordKey(), fromUtf8(request.zoomPassword)},
            {zoomNotAvailableKey(), request.zoomNotAvailable},
            {signatureImageKey(), encodedSignatureImage(request.signatureImage)},
            {signatureModeKey(), normalizedSignatureMode(request.signatureMode)},
            {typedSignatureTextKey(), fromUtf8(request.typedSignatureText)},
            {typedSignatureFontKey(), normalizedSignatureFont(
                request.typedSignatureFont
                )}
        };

        const RepositoryStatus saved = repository->saveSettings(values);
        if (!saved)
        {
            return Application::PersonalDetailsSaveResult::failure(
                repositoryError(
                    QStringLiteral("Personal details could not be saved"),
                    saved.error()
                    )
                );
        }

        return Application::PersonalDetailsSaveResult::success();
    }

private:
    template <typename Loader>
    [[nodiscard]] static QVariant loadWithLegacyFallback(
        Loader&& load,
        const QString& primaryKey,
        const QString& legacyKey,
        const QVariant& defaultValue
        )
    {
        const QVariant primary = load(primaryKey, QVariant{});
        return primary.isValid()
            ? primary
            : load(legacyKey, defaultValue);
    }

    static void migrateLegacyZoomSetting(
        SettingsRepository* repository,
        const QString& primaryKey,
        const QString& legacyKey
        )
    {
        const auto primary = repository->loadSetting(primaryKey);
        if (!primary && primary.error().code != RepositoryErrorCode::NotFound)
        {
            return;
        }
        if (primary && primary->isValid())
        {
            return;
        }

        const auto legacy = repository->loadSetting(legacyKey);
        if (legacy && legacy->isValid())
        {
            static_cast<void>(repository->saveSetting(primaryKey, *legacy));
        }
    }

    [[nodiscard]] static Application::PersonalSignatureFontChoice
    signatureFontChoice(const QVariant& value)
    {
        bool integerSucceeded = false;
        const int storedFont = value.toInt(&integerSucceeded);
        bool numericSucceeded = false;
        const double numericFont = value.toDouble(&numericSucceeded);
        if (!integerSucceeded || !numericSucceeded
            || numericFont != static_cast<double>(storedFont))
        {
            return Application::PersonalSignatureFontChoice::JustAnotherHand;
        }

        switch (storedFont)
        {
        case 1:
            return Application::PersonalSignatureFontChoice::DancingScript;
        case 2:
            return Application::PersonalSignatureFontChoice::GreatVibes;
        case 3:
            return Application::PersonalSignatureFontChoice::Caveat;
        case 0:
        default:
            return Application::PersonalSignatureFontChoice::JustAnotherHand;
        }
    }

    [[nodiscard]] static QString encodedSignatureImage(
        const Domain::PersonalSignatureImageSaveBytes& image
        )
    {
        if (image.empty())
        {
            return {};
        }

        const QByteArray prepared = SignatureImage::prepareForEmbedding(
            QByteArray(
                image.value().data(),
                static_cast<qsizetype>(image.value().size())
                )
            );
        return QString::fromLatin1(prepared.toBase64());
    }

    [[nodiscard]] static QString fromUtf8(const std::string& value)
    {
        return QString::fromUtf8(
            value.data(),
            static_cast<qsizetype>(value.size())
            );
    }

    [[nodiscard]] static std::string utf8(const QByteArray& value)
    {
        return std::string(
            value.constData(),
            static_cast<std::size_t>(value.size())
            );
    }

    [[nodiscard]] static std::string bytes(const QByteArray& value)
    {
        return value.isEmpty()
            ? std::string{}
            : std::string(
                  value.constData(),
                  static_cast<std::size_t>(value.size())
                  );
    }

    [[nodiscard]] static int normalizedSignatureMode(
        Application::PersonalSignatureMode mode
        )
    {
        return mode == Application::PersonalSignatureMode::Type ? 1 : 0;
    }

    [[nodiscard]] static int normalizedSignatureFont(
        Application::PersonalSignatureFontChoice font
        )
    {
        switch (font)
        {
        case Application::PersonalSignatureFontChoice::JustAnotherHand:
            return 0;
        case Application::PersonalSignatureFontChoice::DancingScript:
            return 1;
        case Application::PersonalSignatureFontChoice::GreatVibes:
            return 2;
        case Application::PersonalSignatureFontChoice::Caveat:
            return 3;
        }
        return 0;
    }

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

    [[nodiscard]] static QString legacyZoomLoginIdKey()
    {
        return QStringLiteral("subPrep/personalZoomEmail");
    }

    [[nodiscard]] static QString legacyZoomPasswordKey()
    {
        return QStringLiteral("subPrep/personalZoomPassword");
    }

    [[nodiscard]] static QString legacyZoomNotAvailableKey()
    {
        return QStringLiteral("subPrep/personalZoomNotAvailable");
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
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError repositoryError(
        const QString& operation,
        const RepositoryError& error
        )
    {
        const QByteArray diagnostic = toLegacyDiagnostic(error).toUtf8();
        std::string message;
        const QByteArray operationBytes = operation.toUtf8();
        message.append(
            operationBytes.constData(),
            static_cast<std::size_t>(operationBytes.size())
            );
        if (!diagnostic.isEmpty())
        {
            message.append(": ");
            message.append(
                diagnostic.constData(),
                static_cast<std::size_t>(diagnostic.size())
                );
        }
        message.push_back('.');
        return {
            .code = repositoryDomainErrorCode(error.code),
            .message = std::move(message),
            .recoverable = true
        };
    }

    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform
