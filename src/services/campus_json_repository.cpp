#include "campus_json_repository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QObject>
#include <QRegularExpression>
#include <QSaveFile>

#include <algorithm>
#include <utility>

namespace
{
QString valueString(
    const QJsonObject& object,
    const QString& key
    )
{
    return object.value(key).toString();
}

QStringList valueStringList(
    const QJsonObject& object,
    const QString& key
    )
{
    QStringList values;

    const QJsonArray array =
        object.value(key).toArray();

    for (const QJsonValue& value : array)
    {
        values.append(
            value.toString()
            );
    }

    return values;
}

QString combinedCityDistrict(
    const QString& city,
    const QString& district
    )
{
    return QStringList{
        city.trimmed(),
        district.trimmed()
        }
        .join(QStringLiteral(" "))
        .simplified();
}

QJsonObject normalizedAddressObject(
    const QJsonObject& object,
    const QString& fallbackCompleteAddress = QString()
    )
{
    QString city =
        valueString(
            object,
            QStringLiteral("city")
            ).trimmed();

    QString district =
        valueString(
            object,
            QStringLiteral("district")
            ).trimmed();

    const QString legacyCityDistrict =
        valueString(
            object,
            QStringLiteral("city_district")
            ).trimmed();

    if (city.isEmpty() && district.isEmpty() && !legacyCityDistrict.isEmpty())
    {
        const int splitIndex =
            legacyCityDistrict.lastIndexOf(u' ');

        if (splitIndex > 0)
        {
            city =
                legacyCityDistrict.left(splitIndex).trimmed();
            district =
                legacyCityDistrict.mid(splitIndex + 1).trimmed();
        }
        else
        {
            district =
                legacyCityDistrict;
        }
    }

    QString line1 =
        valueString(
            object,
            QStringLiteral("line1")
            );

    const bool hasStructuredAddress =
        !valueString(object, QStringLiteral("province")).trimmed().isEmpty()
        || !city.isEmpty()
        || !district.isEmpty()
        || !line1.trimmed().isEmpty()
        || !valueString(object, QStringLiteral("line2")).trimmed().isEmpty()
        || !valueString(object, QStringLiteral("postal_code")).trimmed().isEmpty()
        || !valueString(object, QStringLiteral("addr_note")).trimmed().isEmpty();

    if (!hasStructuredAddress && !fallbackCompleteAddress.trimmed().isEmpty())
    {
        line1 =
            fallbackCompleteAddress.trimmed();
    }

    QJsonObject address;

    address.insert(
        QStringLiteral("province"),
        valueString(
            object,
            QStringLiteral("province")
            )
        );

    address.insert(
        QStringLiteral("city"),
        city
        );

    address.insert(
        QStringLiteral("district"),
        district
        );

    address.insert(
        QStringLiteral("city_district"),
        combinedCityDistrict(city, district)
        );

    address.insert(
        QStringLiteral("line1"),
        line1
        );

    address.insert(
        QStringLiteral("line2"),
        valueString(
            object,
            QStringLiteral("line2")
            )
        );

    address.insert(
        QStringLiteral("postal_code"),
        valueString(
            object,
            QStringLiteral("postal_code")
            )
        );

    address.insert(
        QStringLiteral("addr_note"),
        valueString(
            object,
            QStringLiteral("addr_note")
            )
        );

    const QJsonObject modernAddress =
        object
            .value(QStringLiteral("modern"))
            .toObject();

    if (!modernAddress.isEmpty())
    {
        address.insert(
            QStringLiteral("modern"),
            normalizedAddressObject(modernAddress)
            );
    }

    const QJsonObject classicAddress =
        object
            .value(QStringLiteral("classic"))
            .toObject();

    if (!classicAddress.isEmpty())
    {
        address.insert(
            QStringLiteral("classic"),
            normalizedAddressObject(classicAddress)
            );
    }

    const QString addressSystem =
        valueString(
            object,
            QStringLiteral("address_system")
            );

    if (!addressSystem.trimmed().isEmpty())
    {
        address.insert(
            QStringLiteral("address_system"),
            addressSystem
            );
    }

    return address;
}

QJsonObject directionsLanguageObject(
    const QString& buildingName,
    const QJsonObject& address
    )
{
    QJsonObject object =
        normalizedAddressObject(address);

    object.insert(
        QStringLiteral("building_name"),
        buildingName
        );

    return object;
}

bool isDefaultCampus(
    const CampusInfo& campus
    )
{
    return campus.id.compare(QStringLiteral("default"), Qt::CaseInsensitive) == 0
        || campus.campusName.compare(QStringLiteral("Default"), Qt::CaseInsensitive) == 0;
}

bool isDefaultCampusFile(
    const QString& filePath
    )
{
    return QFileInfo(filePath)
        .baseName()
        .compare(QStringLiteral("default"), Qt::CaseInsensitive) == 0;
}

QJsonObject campusToJson(
    const CampusInfo& campus
    )
{
    QJsonObject object;

    object.insert(
        QStringLiteral("id"),
        campus.id
        );

    object.insert(
        QStringLiteral("campus_name"),
        campus.campusName
        );

    object.insert(
        QStringLiteral("campus_code"),
        campus.campusCode
        );

    object.insert(
        QStringLiteral("building_name"),
        campus.buildingName
        );

    object.insert(
        QStringLiteral("address"),
        campus.address
        );

    QJsonObject directions;

    directions.insert(
        QStringLiteral("en"),
        directionsLanguageObject(
            campus.buildingName,
            campus.directionsAddressEn
            )
        );

    directions.insert(
        QStringLiteral("kr"),
        directionsLanguageObject(
            campus.buildingNameKr,
            campus.directionsAddressKr
            )
        );

    object.insert(
        QStringLiteral("directions"),
        directions
        );

    object.insert(
        QStringLiteral("phone_number"),
        campus.phoneNumber
        );

    object.insert(
        QStringLiteral("office_number"),
        campus.officeNumber
        );

    QJsonArray transitSteps;

    for (const QString& step : campus.transitSteps)
    {
        transitSteps.append(step);
    }

    object.insert(
        QStringLiteral("transit_steps"),
        transitSteps
        );

    object.insert(
        QStringLiteral("arrival_info"),
        campus.arrivalInfo
        );

    object.insert(
        QStringLiteral("image_main"),
        campus.imageMain
        );

    object.insert(
        QStringLiteral("office_wifi"),
        campus.officeWifi
        );

    object.insert(
        QStringLiteral("office_wifi_password"),
        campus.officeWifiPassword
        );

    object.insert(
        QStringLiteral("printer_name"),
        campus.printerName
        );

    object.insert(
        QStringLiteral("printer_steps"),
        campus.printerSteps
        );

    object.insert(
        QStringLiteral("printer_driver_url"),
        campus.printerDriverUrl
        );

    object.insert(
        QStringLiteral("printer_driver_url_unavailable"),
        campus.printerDriverUrlUnavailable
        );

    object.insert(
        QStringLiteral("photocopier_code"),
        campus.photocopierCode
        );

    object.insert(
        QStringLiteral("housing_locations"),
        campus.housingLocations
        );

    return object;
}

CampusInfo campusFromJson(
    const QJsonObject& object
    )
{
    CampusInfo campus;

    campus.id =
        valueString(
            object,
            QStringLiteral("id")
            );

    campus.campusName =
        valueString(
            object,
            QStringLiteral("campus_name")
            );

    campus.campusCode =
        valueString(
            object,
            QStringLiteral("campus_code")
            );

    campus.buildingName =
        valueString(
            object,
            QStringLiteral("building_name")
            );

    campus.address =
        valueString(
            object,
            QStringLiteral("address")
            );

    const QJsonObject directions =
        object
            .value(QStringLiteral("directions"))
            .toObject();

    const QJsonObject englishDirections =
        directions
            .value(QStringLiteral("en"))
            .toObject();

    const QJsonObject koreanDirections =
        directions
            .value(QStringLiteral("kr"))
            .toObject();

    const QString englishBuildingName =
        valueString(
            englishDirections,
            QStringLiteral("building_name")
            );

    if (!englishBuildingName.trimmed().isEmpty())
    {
        campus.buildingName =
            englishBuildingName;
    }

    campus.buildingNameKr =
        valueString(
            koreanDirections,
            QStringLiteral("building_name")
            );

    campus.directionsAddressEn =
        normalizedAddressObject(
            englishDirections,
            campus.address
            );

    campus.directionsAddressKr =
        normalizedAddressObject(koreanDirections);

    campus.phoneNumber =
        valueString(
            object,
            QStringLiteral("phone_number")
            );

    campus.officeNumber =
        valueString(
            object,
            QStringLiteral("office_number")
            );

    campus.transitSteps =
        valueStringList(
            object,
            QStringLiteral("transit_steps")
            );

    campus.arrivalInfo =
        valueString(
            object,
            QStringLiteral("arrival_info")
            );

    campus.imageMain =
        valueString(
            object,
            QStringLiteral("image_main")
            );

    campus.officeWifi =
        valueString(
            object,
            QStringLiteral("office_wifi")
            );

    campus.officeWifiPassword =
        valueString(
            object,
            QStringLiteral("office_wifi_password")
            );

    campus.printerName =
        valueString(
            object,
            QStringLiteral("printer_name")
            );

    campus.printerSteps =
        valueString(
            object,
            QStringLiteral("printer_steps")
            );

    campus.printerDriverUrl =
        valueString(
            object,
            QStringLiteral("printer_driver_url")
            );

    campus.printerDriverUrlUnavailable =
        object.contains(QStringLiteral("printer_driver_url_unavailable"))
            ? object
                .value(QStringLiteral("printer_driver_url_unavailable"))
                .toBool(true)
            : true;

    campus.photocopierCode =
        valueString(
            object,
            QStringLiteral("photocopier_code")
            );

    campus.housingLocations =
        object.value(
            QStringLiteral("housing_locations")
            ).toArray();

    if (campus.id.trimmed().isEmpty())
    {
        campus.id =
            CampusJsonRepository::idFromName(
                campus.campusName
                );
    }

    return campus;
}

CampusInfo placeholderCampus()
{
    CampusInfo campus;

    campus.id =
        QStringLiteral("placeholder");
    campus.campusName =
        QStringLiteral("Placeholder");
    campus.campusCode =
        QStringLiteral("PLH");
    campus.buildingName =
        QStringLiteral("Sample Learning Center");
    campus.buildingNameKr =
        QStringLiteral("샘플 학습 센터");
    campus.address =
        QStringLiteral("123 Example-ro, Sample District, Seoul");
    campus.phoneNumber =
        QStringLiteral("02-0000-0000");
    campus.officeNumber =
        QStringLiteral("Room 401");
    campus.transitSteps =
        {
            QStringLiteral("Take Line 2 to Sample Station."),
            QStringLiteral("Use Exit 3 and walk straight for two blocks."),
            QStringLiteral("Turn right at the coffee shop and enter the glass-front building.")
        };
    campus.arrivalInfo =
        QStringLiteral("Check in at the front desk and ask for the ClassMngr sample classroom.");
    campus.imageMain =
        QStringLiteral("Sample map image path or URL");
    campus.officeWifi =
        QStringLiteral("Placeholder-Office-WiFi");
    campus.officeWifiPassword =
        QStringLiteral("sample-password");
    campus.printerName =
        QStringLiteral("Placeholder Printer");
    campus.printerSteps =
        QStringLiteral("Open system printer settings.\nAdd a network printer.\nChoose Placeholder Printer.");
    campus.printerDriverUrlUnavailable =
        true;
    campus.photocopierCode =
        QStringLiteral("0000");

    campus.directionsAddressEn.insert(
        QStringLiteral("province"),
        QStringLiteral("Seoul")
        );
    campus.directionsAddressEn.insert(
        QStringLiteral("city"),
        QString()
        );
    campus.directionsAddressEn.insert(
        QStringLiteral("district"),
        QStringLiteral("Jongno-gu")
        );
    campus.directionsAddressEn.insert(
        QStringLiteral("city_district"),
        QStringLiteral("Jongno-gu")
        );
    campus.directionsAddressEn.insert(
        QStringLiteral("line1"),
        QStringLiteral("23 Sajik-ro-3-gil")
        );
    campus.directionsAddressEn.insert(
        QStringLiteral("line2"),
        QStringLiteral("102-dong 304-ho")
        );
    campus.directionsAddressEn.insert(
        QStringLiteral("postal_code"),
        QStringLiteral("30174")
        );
    campus.directionsAddressEn.insert(
        QStringLiteral("addr_note"),
        QStringLiteral("Check in at the front desk.")
        );

    campus.directionsAddressKr.insert(
        QStringLiteral("province"),
        QStringLiteral("서울특별시")
        );
    campus.directionsAddressKr.insert(
        QStringLiteral("city"),
        QString()
        );
    campus.directionsAddressKr.insert(
        QStringLiteral("district"),
        QStringLiteral("종로구")
        );
    campus.directionsAddressKr.insert(
        QStringLiteral("city_district"),
        QStringLiteral("종로구")
        );
    campus.directionsAddressKr.insert(
        QStringLiteral("line1"),
        QStringLiteral("사직로3길 23")
        );
    campus.directionsAddressKr.insert(
        QStringLiteral("line2"),
        QStringLiteral("102동 304호")
        );
    campus.directionsAddressKr.insert(
        QStringLiteral("postal_code"),
        QStringLiteral("30174")
        );
    campus.directionsAddressKr.insert(
        QStringLiteral("addr_note"),
        QStringLiteral("프런트 데스크에서 체크인하세요.")
        );

    QJsonObject englishAddress;
    englishAddress.insert(
        QStringLiteral("province"),
        QStringLiteral("Seoul")
        );
    englishAddress.insert(
        QStringLiteral("city_district"),
        QStringLiteral("Sample-gu")
        );
    englishAddress.insert(
        QStringLiteral("city"),
        QString()
        );
    englishAddress.insert(
        QStringLiteral("district"),
        QStringLiteral("Sample-gu")
        );
    englishAddress.insert(
        QStringLiteral("line1"),
        QStringLiteral("45 Sample Housing Road")
        );
    englishAddress.insert(
        QStringLiteral("line2"),
        QStringLiteral("Apt. 101")
        );
    englishAddress.insert(
        QStringLiteral("postal_code"),
        QStringLiteral("00000")
        );
    englishAddress.insert(
        QStringLiteral("addr_note"),
        QStringLiteral("Use the south entrance after 8 PM.")
        );

    QJsonObject koreanAddress;
    koreanAddress.insert(
        QStringLiteral("province"),
        QStringLiteral("서울특별시")
        );
    koreanAddress.insert(
        QStringLiteral("city_district"),
        QStringLiteral("종로구")
        );
    koreanAddress.insert(
        QStringLiteral("city"),
        QString()
        );
    koreanAddress.insert(
        QStringLiteral("district"),
        QStringLiteral("종로구")
        );
    koreanAddress.insert(
        QStringLiteral("line1"),
        QStringLiteral("사직로3길 23")
        );
    koreanAddress.insert(
        QStringLiteral("line2"),
        QStringLiteral("102동 304호")
        );
    koreanAddress.insert(
        QStringLiteral("postal_code"),
        QStringLiteral("30174")
        );
    koreanAddress.insert(
        QStringLiteral("addr_note"),
        QStringLiteral("오후 8시 이후에는 남문을 이용하세요.")
        );

    QJsonObject housing;
    housing.insert(
        QStringLiteral("name"),
        QStringLiteral("Placeholder Housing")
        );
    housing.insert(
        QStringLiteral("en"),
        englishAddress
        );
    housing.insert(
        QStringLiteral("kr"),
        koreanAddress
        );

    campus.housingLocations.append(housing);

    return campus;
}
} // namespace

CampusJsonRepository::CampusJsonRepository(
    QString directoryPath
    )
    : m_directoryPath(std::move(directoryPath))
{
}

void CampusJsonRepository::ensurePlaceholderCampus() const
{
    const QString placeholderPath =
        filePathForCampusId(
            QStringLiteral("placeholder")
            );

    if (QFile::exists(placeholderPath))
    {
        return;
    }

    saveCampus(
        placeholderCampus()
        );
}

QList<CampusInfo>
CampusJsonRepository::loadCampuses() const
{
    QList<CampusInfo> campuses;

    const QDir directory(m_directoryPath);

    const QStringList files =
        directory.entryList(
            {QStringLiteral("*.json")},
            QDir::Files,
            QDir::Name
            );

    for (const QString& file : files)
    {
        const QString filePath =
            directory.filePath(file);

        if (isDefaultCampusFile(filePath))
        {
            continue;
        }

        const std::optional<CampusInfo> campus =
            readCampusFile(
                filePath
                );

        if (campus.has_value() && !isDefaultCampus(campus.value()))
        {
            campuses.append(
                campus.value()
                );
        }
    }

    std::ranges::sort(
        campuses,
        [](const CampusInfo& lhs, const CampusInfo& rhs)
        {
            return QString::compare(
                lhs.campusName,
                rhs.campusName,
                Qt::CaseInsensitive
                ) < 0;
        }
        );

    return campuses;
}

std::optional<CampusInfo>
CampusJsonRepository::loadCampus(
    const QString& campusId
    ) const
{
    if (campusId.compare(QStringLiteral("default"), Qt::CaseInsensitive) == 0)
    {
        return std::nullopt;
    }

    return readCampusFile(
        filePathForCampusId(campusId)
        );
}

bool CampusJsonRepository::saveCampus(
    const CampusInfo& campus,
    QString* errorMessage
    ) const
{
    if (!ensureDirectory(errorMessage))
    {
        return false;
    }

    CampusInfo campusToSave = campus;

    if (campusToSave.id.trimmed().isEmpty())
    {
        campusToSave.id =
            idFromName(
                campusToSave.campusName
                );
    }

    QSaveFile file(
        filePathForCampusId(campusToSave.id)
        );

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        if (errorMessage)
        {
            *errorMessage =
                file.errorString();
        }

        return false;
    }

    const QJsonDocument document(
        campusToJson(campusToSave)
        );

    file.write(
        document.toJson(QJsonDocument::Indented)
        );

    if (!file.commit())
    {
        if (errorMessage)
        {
            *errorMessage =
                file.errorString();
        }

        return false;
    }

    return true;
}

QString CampusJsonRepository::filePathForCampusId(
    const QString& campusId
    ) const
{
    return QDir(m_directoryPath)
        .filePath(
            idFromName(campusId) + QStringLiteral(".json")
            );
}

QString CampusJsonRepository::idFromName(
    const QString& name
    )
{
    QString id =
        name.trimmed().toLower();

    id.replace(
        QRegularExpression(QStringLiteral("[^a-z0-9]+")),
        QStringLiteral("_")
        );

    id.replace(
        QRegularExpression(QStringLiteral("^_+|_+$")),
        QString()
        );

    if (id.isEmpty())
    {
        id =
            QStringLiteral("campus");
    }

    return id;
}

bool CampusJsonRepository::ensureDirectory(
    QString* errorMessage
    ) const
{
    QDir directory(m_directoryPath);

    if (directory.exists())
    {
        return true;
    }

    if (directory.mkpath(QStringLiteral(".")))
    {
        return true;
    }

    if (errorMessage)
    {
        *errorMessage =
            QObject::tr("Unable to create campus data directory.");
    }

    return false;
}

std::optional<CampusInfo> CampusJsonRepository::readCampusFile(
    const QString& filePath
    ) const
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return std::nullopt;
    }

    QJsonParseError parseError;

    const QJsonDocument document =
        QJsonDocument::fromJson(
            file.readAll(),
            &parseError
            );

    if (
        parseError.error != QJsonParseError::NoError
        || !document.isObject()
        )
    {
        return std::nullopt;
    }

    return campusFromJson(
        document.object()
        );
}
