#include "campus_json_repository.h"

#include <QDir>
#include <QFile>
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
        QStringLiteral("building_name"),
        campus.buildingName
        );

    object.insert(
        QStringLiteral("address"),
        campus.address
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
    campus.buildingName =
        QStringLiteral("Sample Learning Center");
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
    campus.photocopierCode =
        QStringLiteral("0000");

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
        QStringLiteral("Sample Korean Province")
        );
    koreanAddress.insert(
        QStringLiteral("city_district"),
        QStringLiteral("Sample Korean District")
        );
    koreanAddress.insert(
        QStringLiteral("line1"),
        QStringLiteral("45 Sample Korean Housing Road")
        );
    koreanAddress.insert(
        QStringLiteral("line2"),
        QStringLiteral("Unit 101")
        );
    koreanAddress.insert(
        QStringLiteral("postal_code"),
        QStringLiteral("00000")
        );
    koreanAddress.insert(
        QStringLiteral("addr_note"),
        QStringLiteral("Use the south entrance after 8 PM.")
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
        const std::optional<CampusInfo> campus =
            readCampusFile(
                directory.filePath(file)
                );

        if (campus.has_value())
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
