#include "personal_details_repository.h"

#include "data/data_service.h"
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

const QString LegacyZoomEmailKey =
    QStringLiteral("subPrep/personalZoomEmail");
const QString LegacyZoomPasswordKey =
    QStringLiteral("subPrep/personalZoomPassword");
const QString LegacyZoomNotAvailableKey =
    QStringLiteral("subPrep/personalZoomNotAvailable");

QVariant loadWithLegacyFallback(
    const PersonalDetailsRepository& repository,
    const QString& primaryKey,
    const QString& legacyKey,
    const QVariant& defaultValue
    )
{
    QVariant value = repository.loadSetting(primaryKey, QVariant());

    if (value.isValid())
    {
        return value;
    }

    value = repository.loadSetting(legacyKey, QVariant());

    if (value.isValid())
    {
        repository.saveSetting(primaryKey, value);
        return value;
    }

    return defaultValue;
}
} // namespace

PersonalDetailsRepository::PersonalDetailsRepository(DataService* dataService)
    : m_dataService(dataService)
{
}

PersonalDetailsRepository::PersonalDetailsRepository(SettingsService* settingsService)
    : m_settingsService(settingsService)
{
}

PersonalDetails PersonalDetailsRepository::load() const
{
    PersonalDetails details;

    if (!isAvailable())
    {
        return details;
    }

    details.name = loadSetting(NameKey, QString()).toString();
    details.campus = loadSetting(CampusKey, QString()).toString();
    details.zoomLoginId = loadWithLegacyFallback(
        *this,
        ZoomLoginIdKey,
        LegacyZoomEmailKey,
        QStringLiteral("N/A")
        ).toString();
    details.zoomPassword = loadWithLegacyFallback(
        *this,
        ZoomPasswordKey,
        LegacyZoomPasswordKey,
        QStringLiteral("N/A")
        ).toString();
    details.zoomNotAvailable = loadWithLegacyFallback(
        *this,
        ZoomNotAvailableKey,
        LegacyZoomNotAvailableKey,
        true
        ).toBool();
    details.signatureImage =
        SignatureImage::prepareForEmbedding(
            QByteArray::fromBase64(
                loadSetting(SignatureImageKey, QString())
                    .toString()
                    .toLatin1()
                )
            );
    return details;
}

bool PersonalDetailsRepository::save(const PersonalDetails& details) const
{
    if (!isAvailable())
    {
        return false;
    }

    saveSetting(NameKey, details.name);
    saveSetting(CampusKey, details.campus);
    saveSetting(ZoomLoginIdKey, details.zoomLoginId);
    saveSetting(ZoomPasswordKey, details.zoomPassword);
    saveSetting(
        ZoomNotAvailableKey,
        details.zoomNotAvailable
        );
    saveSetting(
        SignatureImageKey,
        QString::fromLatin1(
            SignatureImage::prepareForEmbedding(
                details.signatureImage
                ).toBase64()
            )
        );
    return true;
}

void PersonalDetailsRepository::saveCampus(const QString& campus) const
{
    if (isAvailable())
    {
        saveSetting(CampusKey, campus);
    }
}

bool PersonalDetailsRepository::isAvailable() const
{
    return (m_dataService && m_dataService->isOpen())
        || (m_settingsService && m_settingsService->isAvailable());
}

QVariant PersonalDetailsRepository::loadSetting(
    const QString& key,
    const QVariant& defaultValue
    ) const
{
    return m_settingsService
        ? m_settingsService->load(key, defaultValue)
        : m_dataService
            ? m_dataService->loadSetting(key, defaultValue)
            : defaultValue;
}

void PersonalDetailsRepository::saveSetting(
    const QString& key,
    const QVariant& value
    ) const
{
    if (m_settingsService)
        m_settingsService->save(key, value);
    else if (m_dataService)
        m_dataService->saveSetting(key, value);
}
