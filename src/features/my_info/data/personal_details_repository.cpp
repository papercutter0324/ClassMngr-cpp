#include "personal_details_repository.h"

#include "app/services/feature_services.h"
#include "signature_image_processor.h"

#include <QVariant>

namespace
{
const QString NameKey = QStringLiteral("myInfo/name");
const QString CampusKey = QStringLiteral("myInfo/campus");
const QString ZoomLoginIdKey = QStringLiteral("myInfo/zoomLoginId");
const QString ZoomPasswordKey = QStringLiteral("myInfo/zoomPassword");
const QString ZoomNotAvailableKey = QStringLiteral("myInfo/zoomNotAvailable");
const QString SignatureImageKey = QStringLiteral("myInfo/signatureImage");
const QString SignatureModeKey = QStringLiteral("myInfo/signatureMode");
const QString TypedSignatureTextKey = QStringLiteral("myInfo/typedSignatureText");
const QString TypedSignatureFontKey = QStringLiteral("myInfo/typedSignatureFont");

const QString LegacyZoomEmailKey =
    QStringLiteral("subPrep/personalZoomEmail");
const QString LegacyZoomPasswordKey =
    QStringLiteral("subPrep/personalZoomPassword");
const QString LegacyZoomNotAvailableKey =
    QStringLiteral("subPrep/personalZoomNotAvailable");

const QString UnavailableSettingsDiagnostic =
    QStringLiteral("No Teacher Profile service is available.");

RepositoryResult<QVariant> loadWithLegacyFallback(
    const PersonalDetailsRepository& repository,
    const QString& primaryKey,
    const QString& legacyKey,
    const QVariant& defaultValue
    )
{
    RepositoryResult<QVariant> value = repository.loadSetting(
        primaryKey,
        QVariant()
        );
    if (!value)
    {
        return std::unexpected(value.error());
    }

    if (value->isValid())
    {
        return value;
    }

    value = repository.loadSetting(legacyKey, QVariant());
    if (!value)
    {
        return std::unexpected(value.error());
    }

    if (value->isValid())
    {
        [[maybe_unused]] const RepositoryStatus migrated =
            repository.saveSetting(primaryKey, *value);
        return value;
    }

    return defaultValue;
}
} // namespace

PersonalDetailsRepository::PersonalDetailsRepository(SettingsService* settingsService)
    : m_settingsService(settingsService)
{
}

RepositoryResult<PersonalDetails> PersonalDetailsRepository::load() const
{
    PersonalDetails details;

    const RepositoryResult<QVariant> name = loadSetting(NameKey, QString());
    if (!name)
    {
        return std::unexpected(name.error());
    }
    details.name = name->toString();

    const RepositoryResult<QVariant> campus = loadSetting(CampusKey, QString());
    if (!campus)
    {
        return std::unexpected(campus.error());
    }
    details.campus = campus->toString();

    const RepositoryResult<QVariant> zoomLoginId = loadWithLegacyFallback(
        *this,
        ZoomLoginIdKey,
        LegacyZoomEmailKey,
        QStringLiteral("N/A")
        );
    if (!zoomLoginId)
    {
        return std::unexpected(zoomLoginId.error());
    }
    details.zoomLoginId = zoomLoginId->toString();

    const RepositoryResult<QVariant> zoomPassword = loadWithLegacyFallback(
        *this,
        ZoomPasswordKey,
        LegacyZoomPasswordKey,
        QStringLiteral("N/A")
        );
    if (!zoomPassword)
    {
        return std::unexpected(zoomPassword.error());
    }
    details.zoomPassword = zoomPassword->toString();

    const RepositoryResult<QVariant> zoomNotAvailable = loadWithLegacyFallback(
        *this,
        ZoomNotAvailableKey,
        LegacyZoomNotAvailableKey,
        true
        );
    if (!zoomNotAvailable)
    {
        return std::unexpected(zoomNotAvailable.error());
    }
    details.zoomNotAvailable = zoomNotAvailable->toBool();

    const RepositoryResult<QVariant> signatureImage =
        loadSetting(SignatureImageKey, QString());
    if (!signatureImage)
    {
        return std::unexpected(signatureImage.error());
    }
    details.signatureImage =
        SignatureImage::prepareForEmbedding(
            QByteArray::fromBase64(
                signatureImage->toString()
                    .toLatin1()
                )
            );

    const RepositoryResult<QVariant> signatureMode = loadSetting(
        SignatureModeKey,
        static_cast<int>(SignatureMode::Image)
        );
    if (!signatureMode)
    {
        return std::unexpected(signatureMode.error());
    }
    details.signatureMode = signatureMode->toInt()
            == static_cast<int>(SignatureMode::Type)
            ? SignatureMode::Type
            : SignatureMode::Image;

    const RepositoryResult<QVariant> typedSignatureText = loadSetting(
        TypedSignatureTextKey,
        QString()
        );
    if (!typedSignatureText)
    {
        return std::unexpected(typedSignatureText.error());
    }
    details.typedSignatureText = typedSignatureText->toString();

    const RepositoryResult<QVariant> typedSignatureFont = loadSetting(
        TypedSignatureFontKey,
        0
        );
    if (!typedSignatureFont)
    {
        return std::unexpected(typedSignatureFont.error());
    }
    details.typedSignatureFont = typedSignatureFont->toInt();

    return details;
}

RepositoryStatus PersonalDetailsRepository::save(
    const PersonalDetails& details
    ) const
{
    if (!m_settingsService)
    {
        return std::unexpected(RepositoryError{UnavailableSettingsDiagnostic});
    }

    const QVariantMap settings = {
        {NameKey, details.name},
        {CampusKey, details.campus},
        {ZoomLoginIdKey, details.zoomLoginId},
        {ZoomPasswordKey, details.zoomPassword},
        {ZoomNotAvailableKey, details.zoomNotAvailable},
        {
            SignatureModeKey,
            static_cast<int>(details.signatureMode)
        },
        {TypedSignatureTextKey, details.typedSignatureText},
        {TypedSignatureFontKey, details.typedSignatureFont},
        {
            SignatureImageKey,
            QString::fromLatin1(
                SignatureImage::prepareForEmbedding(
                    details.signatureImage
                    ).toBase64()
                )
        }
    };

    const Status saved = m_settingsService->saveAll(settings);
    if (!saved)
    {
        return std::unexpected(RepositoryError{saved.error()});
    }
    return {};
}

RepositoryStatus PersonalDetailsRepository::saveCampus(
    const QString& campus
    ) const
{
    if (!isAvailable())
    {
        return std::unexpected(RepositoryError{UnavailableSettingsDiagnostic});
    }

    return saveSetting(CampusKey, campus);
}

bool PersonalDetailsRepository::isAvailable() const
{
    return m_settingsService
        && m_settingsService->isAvailable();
}

RepositoryResult<QVariant> PersonalDetailsRepository::loadSetting(
    const QString& key,
    const QVariant& defaultValue
    ) const
{
    if (!m_settingsService)
    {
        return std::unexpected(RepositoryError{UnavailableSettingsDiagnostic});
    }

    const Result<QVariant> loaded = m_settingsService->load(key);
    if (!loaded)
    {
        return std::unexpected(RepositoryError{loaded.error()});
    }

    return loaded->isValid() ? *loaded : defaultValue;
}

RepositoryStatus PersonalDetailsRepository::saveSetting(
    const QString& key,
    const QVariant& value
    ) const
{
    if (!m_settingsService)
    {
        return std::unexpected(RepositoryError{UnavailableSettingsDiagnostic});
    }

    const Status saved = m_settingsService->save(key, value);
    if (!saved)
    {
        return std::unexpected(RepositoryError{saved.error()});
    }
    return {};
}
