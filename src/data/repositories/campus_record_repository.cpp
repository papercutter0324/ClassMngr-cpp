#include "campus_record_repository.h"

#include <QSqlQuery>

namespace
{
CampusRecord campusFromQuery(
    const QSqlQuery& query
    )
{
    CampusRecord campus;

    campus.id =
        query.value("id").toInt();
    campus.name =
        query.value("name").toString();
    campus.buildingName =
        query.value("building_name").toString();
    campus.address =
        query.value("address").toString();
    campus.phoneNumber =
        query.value("phone_number").toString();
    campus.officeNumber =
        query.value("office_number").toString();
    campus.transitSteps =
        query.value("transit_steps").toString();
    campus.arrivalInfo =
        query.value("arrival_info").toString();
    campus.imagePath =
        query.value("image_path").toString();
    campus.officeWifi =
        query.value("office_wifi").toString();
    campus.officeWifiPassword =
        query.value("office_wifi_password").toString();
    campus.printerName =
        query.value("printer_name").toString();
    campus.printerSteps =
        query.value("printer_steps").toString();
    campus.photocopierCode =
        query.value("photocopier_code").toString();
    campus.housingLocations =
        query.value("housing_locations").toString();

    return campus;
}
}

CampusRecordRepository::CampusRecordRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

int CampusRecordRepository::saveCampus(
    const CampusRecord& campus
    )
{
    QSqlQuery query(m_database);

    if (campus.id > 0)
    {
        query.prepare(R"(
            UPDATE campuses
            SET
                name=?,
                building_name=?,
                address=?,
                phone_number=?,
                office_number=?,
                transit_steps=?,
                arrival_info=?,
                image_path=?,
                office_wifi=?,
                office_wifi_password=?,
                printer_name=?,
                printer_steps=?,
                photocopier_code=?,
                housing_locations=?
            WHERE id=?
        )");

        query.addBindValue(campus.name);
        query.addBindValue(campus.buildingName);
        query.addBindValue(campus.address);
        query.addBindValue(campus.phoneNumber);
        query.addBindValue(campus.officeNumber);
        query.addBindValue(campus.transitSteps);
        query.addBindValue(campus.arrivalInfo);
        query.addBindValue(campus.imagePath);
        query.addBindValue(campus.officeWifi);
        query.addBindValue(campus.officeWifiPassword);
        query.addBindValue(campus.printerName);
        query.addBindValue(campus.printerSteps);
        query.addBindValue(campus.photocopierCode);
        query.addBindValue(campus.housingLocations);
        query.addBindValue(campus.id);

        query.exec();

        return campus.id;
    }

    query.prepare(R"(
        INSERT INTO campuses (
            name,
            building_name,
            address,
            phone_number,
            office_number,
            transit_steps,
            arrival_info,
            image_path,
            office_wifi,
            office_wifi_password,
            printer_name,
            printer_steps,
            photocopier_code,
            housing_locations
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");

    query.addBindValue(campus.name);
    query.addBindValue(campus.buildingName);
    query.addBindValue(campus.address);
    query.addBindValue(campus.phoneNumber);
    query.addBindValue(campus.officeNumber);
    query.addBindValue(campus.transitSteps);
    query.addBindValue(campus.arrivalInfo);
    query.addBindValue(campus.imagePath);
    query.addBindValue(campus.officeWifi);
    query.addBindValue(campus.officeWifiPassword);
    query.addBindValue(campus.printerName);
    query.addBindValue(campus.printerSteps);
    query.addBindValue(campus.photocopierCode);
    query.addBindValue(campus.housingLocations);

    query.exec();

    return query.lastInsertId().toInt();
}

CampusRecord CampusRecordRepository::getCampus(
    int campusId
    )
{
    CampusRecord campus;

    QSqlQuery query(m_database);

    query.prepare("SELECT * FROM campuses WHERE id=?");
    query.addBindValue(campusId);
    query.exec();

    if (!query.next())
    {
        return campus;
    }

    return campusFromQuery(query);
}

QList<CampusRecord> CampusRecordRepository::getAllCampuses()
{
    QList<CampusRecord> campuses;

    QSqlQuery query(m_database);

    query.exec(R"(
        SELECT *
        FROM campuses
        ORDER BY name
    )");

    while (query.next())
    {
        campuses.append(
            campusFromQuery(query)
            );
    }

    return campuses;
}

void CampusRecordRepository::deleteCampus(
    int campusId
    )
{
    QSqlQuery query(m_database);

    query.prepare(R"(
        DELETE FROM campuses
        WHERE id=?
    )");

    query.addBindValue(campusId);

    query.exec();
}
