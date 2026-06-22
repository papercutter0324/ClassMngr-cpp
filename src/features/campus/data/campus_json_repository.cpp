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
        !valueString(object, QStringLiteral("building_name")).trimmed().isEmpty()
        || !valueString(object, QStringLiteral("province")).trimmed().isEmpty()
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
        QStringLiteral("building_name"),
        valueString(
            object,
            QStringLiteral("building_name")
            )
        );

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

QString sharedAddressNote(
    const QJsonObject& address
    )
{
    const QString topLevelNote =
        valueString(
            address,
            QStringLiteral("addr_note")
            );

    if (!topLevelNote.trimmed().isEmpty())
    {
        return topLevelNote;
    }

    for (const QString& variant : {
             QStringLiteral("modern"),
             QStringLiteral("classic")
         })
    {
        const QString variantNote =
            valueString(
                address.value(variant).toObject(),
                QStringLiteral("addr_note")
                );

        if (!variantNote.trimmed().isEmpty())
        {
            return variantNote;
        }
    }

    return QString();
}

QJsonObject withoutAddressNotes(
    QJsonObject address
    )
{
    address.remove(QStringLiteral("addr_note"));

    for (const QString& variant : {
             QStringLiteral("modern"),
             QStringLiteral("classic")
         })
    {
        QJsonObject variantAddress =
            address.value(variant).toObject();

        if (!variantAddress.isEmpty())
        {
            variantAddress.remove(
                QStringLiteral("addr_note")
                );
            address.insert(variant, variantAddress);
        }
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

    return withoutAddressNotes(object);
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

    directions.insert(
        QStringLiteral("addr_note"),
        campus.directionsNote
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

    QJsonArray mapImages;

    for (const QString& imagePath : campus.mapImagePaths)
    {
        mapImages.append(imagePath);
    }

    QJsonObject mapLinks;

    mapLinks.insert(
        QStringLiteral("naver"),
        campus.naverMapUrl
        );
    mapLinks.insert(
        QStringLiteral("kakao"),
        campus.kakaoMapUrl
        );

    QJsonObject map;

    map.insert(
        QStringLiteral("images"),
        mapImages
        );
    map.insert(
        QStringLiteral("links"),
        mapLinks
        );

    object.insert(
        QStringLiteral("map"),
        map
        );

    object.insert(
        QStringLiteral("image_main"),
        campus.mapImagePaths.isEmpty()
            ? campus.imageMain
            : campus.mapImagePaths.constFirst()
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

    campus.directionsNote =
        valueString(
            directions,
            QStringLiteral("addr_note")
            );

    if (campus.directionsNote.trimmed().isEmpty())
    {
        campus.directionsNote =
            sharedAddressNote(englishDirections);
    }

    if (campus.directionsNote.trimmed().isEmpty())
    {
        campus.directionsNote =
            sharedAddressNote(koreanDirections);
    }

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

    const QJsonValue mapValue =
        object.value(QStringLiteral("map"));

    const bool hasMapConfiguration =
        mapValue.isObject();

    const QJsonObject map =
        mapValue.toObject();

    const QJsonArray mapImages =
        map
            .value(QStringLiteral("images"))
            .toArray();

    for (const QJsonValue& image : mapImages)
    {
        const QString imagePath =
            image.toString().trimmed();

        if (!imagePath.isEmpty())
        {
            campus.mapImagePaths.append(imagePath);
        }
    }

    if (
        campus.mapImagePaths.isEmpty()
        && !hasMapConfiguration
        && !campus.imageMain.trimmed().isEmpty()
        )
    {
        campus.mapImagePaths.append(campus.imageMain);
    }

    const QJsonObject mapLinks =
        map
            .value(QStringLiteral("links"))
            .toObject();

    campus.naverMapUrl =
        valueString(
            mapLinks,
            QStringLiteral("naver")
            );
    campus.kakaoMapUrl =
        valueString(
            mapLinks,
            QStringLiteral("kakao")
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

} // namespace

CampusJsonRepository::CampusJsonRepository(
    QString directoryPath
    )
    : m_directoryPath(std::move(directoryPath))
{
}

QList<CampusInfo>
CampusJsonRepository::loadCampuses() const
{
    QList<CampusInfo> campuses;

    for (const QString& filePath : campusFilePaths())
    {
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

    const QString campusFilePath =
        filePathForCampusId(campusId);

    return readCampusFile(
        campusFilePath
        );
}

Status CampusJsonRepository::saveCampus(
    const CampusInfo& campus
    ) const
{
    const Status directoryReady =
        ensureDirectory();

    if (!directoryReady)
    {
        return directoryReady;
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
        return std::unexpected(
            QObject::tr("Unable to open campus file for writing:\n%1")
                .arg(file.errorString())
            );
    }

    const QJsonDocument document(
        campusToJson(campusToSave)
        );

    file.write(
        document.toJson(QJsonDocument::Indented)
        );

    if (!file.commit())
    {
        return std::unexpected(
            QObject::tr("Unable to save campus file:\n%1")
                .arg(file.errorString())
            );
    }

    return {};
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

QList<QString> CampusJsonRepository::campusFilePaths() const
{
    QList<QString> filePaths;

    if (m_directoryPath.trimmed().isEmpty())
    {
        return filePaths;
    }

    const QDir directory(m_directoryPath);

    const QStringList files =
        directory.entryList(
            {QStringLiteral("*.json")},
            QDir::Files,
            QDir::Name
            );

    for (const QString& file : files)
    {
        filePaths.append(
            directory.filePath(file)
            );
    }

    return filePaths;
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

Status CampusJsonRepository::ensureDirectory() const
{
    QDir directory(m_directoryPath);

    if (directory.exists())
    {
        return {};
    }

    if (directory.mkpath(QStringLiteral(".")))
    {
        return {};
    }

    return std::unexpected(
        QObject::tr("Unable to create campus data directory.")
        );
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
