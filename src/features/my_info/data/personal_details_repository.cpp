#include "personal_details_repository.h"

#include "data/data_service.h"

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
    DataService* dataService,
    const QString& primaryKey,
    const QString& legacyKey,
    const QVariant& defaultValue
    )
{
    QVariant value = dataService->loadSetting(primaryKey, QVariant());

    if (value.isValid())
    {
        return value;
    }

    value = dataService->loadSetting(legacyKey, QVariant());

    if (value.isValid())
    {
        dataService->saveSetting(primaryKey, value);
        return value;
    }

    return defaultValue;
}
} // namespace

PersonalDetailsRepository::PersonalDetailsRepository(DataService* dataService)
    : m_dataService(dataService)
{
}

PersonalDetails PersonalDetailsRepository::load() const
{
    PersonalDetails details;

    if (!m_dataService || !m_dataService->isOpen())
    {
        return details;
    }

    details.name = m_dataService->loadSetting(NameKey, QString()).toString();
    details.campus = m_dataService->loadSetting(CampusKey, QString()).toString();
    details.zoomLoginId = loadWithLegacyFallback(
        m_dataService,
        ZoomLoginIdKey,
        LegacyZoomEmailKey,
        QStringLiteral("N/A")
        ).toString();
    details.zoomPassword = loadWithLegacyFallback(
        m_dataService,
        ZoomPasswordKey,
        LegacyZoomPasswordKey,
        QStringLiteral("N/A")
        ).toString();
    details.zoomNotAvailable = loadWithLegacyFallback(
        m_dataService,
        ZoomNotAvailableKey,
        LegacyZoomNotAvailableKey,
        true
        ).toBool();
    details.signatureImage = QByteArray::fromBase64(
        m_dataService
            ->loadSetting(SignatureImageKey, QString())
            .toString()
            .toLatin1()
        );
    return details;
}

bool PersonalDetailsRepository::save(const PersonalDetails& details) const
{
    if (!m_dataService || !m_dataService->isOpen())
    {
        return false;
    }

    m_dataService->saveSetting(NameKey, details.name);
    m_dataService->saveSetting(CampusKey, details.campus);
    m_dataService->saveSetting(ZoomLoginIdKey, details.zoomLoginId);
    m_dataService->saveSetting(ZoomPasswordKey, details.zoomPassword);
    m_dataService->saveSetting(
        ZoomNotAvailableKey,
        details.zoomNotAvailable
        );
    m_dataService->saveSetting(
        SignatureImageKey,
        QString::fromLatin1(details.signatureImage.toBase64())
        );
    return true;
}

void PersonalDetailsRepository::saveCampus(const QString& campus) const
{
    if (m_dataService && m_dataService->isOpen())
    {
        m_dataService->saveSetting(CampusKey, campus);
    }
}
