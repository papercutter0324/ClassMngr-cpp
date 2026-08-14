#include "campus_record_repository.h"

#include "data/database/sql_query_utils.h"

#include <QObject>
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

QString campusIdentity(const CampusRecord& campus)
{
    if (campus.id > 0)
    {
        return QObject::tr("campus id %1").arg(campus.id);
    }

    return QObject::tr("campus '%1'").arg(campus.name.trimmed());
}
}

CampusRecordRepository::CampusRecordRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

Result<int> CampusRecordRepository::saveCampus(
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

        const auto executed = SqlQueryUtils::executePrepared(
            query,
            QObject::tr("Updating campus"),
            campusIdentity(campus)
            );
        if (!executed)
        {
            return std::unexpected(executed.error().userMessage());
        }

        if (query.numRowsAffected() == 0)
        {
            return std::unexpected(
                QObject::tr("Updating %1 failed: no matching record exists.")
                    .arg(campusIdentity(campus))
                );
        }

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

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Creating campus"),
        campusIdentity(campus)
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    const int campusId = query.lastInsertId().toInt();
    if (campusId <= 0)
    {
        return std::unexpected(
            QObject::tr(
                "Creating %1 failed: the database did not return a valid "
                "record id."
                ).arg(campusIdentity(campus))
            );
    }

    return campusId;
}

Result<CampusRecord> CampusRecordRepository::getCampus(
    int campusId
    )
{
    if (campusId <= 0)
    {
        return std::unexpected(
            QObject::tr("Loading campus failed: invalid campus id %1.")
                .arg(campusId)
            );
    }

    QSqlQuery query(m_database);

    query.prepare("SELECT * FROM campuses WHERE id=?");
    query.addBindValue(campusId);

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading campus"),
        QObject::tr("campus id %1").arg(campusId)
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    if (!query.next())
    {
        return std::unexpected(
            QObject::tr(
                "Loading campus failed for campus id %1: no matching "
                "record exists."
                ).arg(campusId)
            );
    }

    return campusFromQuery(query);
}

Result<QList<CampusRecord>> CampusRecordRepository::getAllCampuses()
{
    QList<CampusRecord> campuses;

    QSqlQuery query(m_database);

    const auto executed = SqlQueryUtils::execute(
        query,
        QStringLiteral(R"(
        SELECT *
        FROM campuses
        ORDER BY name
    )"),
        QObject::tr("Loading campuses")
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        campuses.append(
            campusFromQuery(query)
            );
    }

    return campuses;
}

Status CampusRecordRepository::deleteCampus(
    int campusId
    )
{
    if (campusId <= 0)
    {
        return std::unexpected(
            QObject::tr("Deleting campus failed: invalid campus id %1.")
                .arg(campusId)
            );
    }

    QSqlQuery query(m_database);

    query.prepare(R"(
        DELETE FROM campuses
        WHERE id=?
    )");

    query.addBindValue(campusId);

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Deleting campus"),
        QObject::tr("campus id %1").arg(campusId)
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    return {};
}
