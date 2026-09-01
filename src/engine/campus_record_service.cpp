#include "classmngr/engine/campus_record_service.h"

#include "classmngr/engine/sqlite_database.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace classmngr::engine
{
namespace
{
Error error(
    ErrorCode code,
    std::string message
    )
{
    return {
        code,
        std::move(message),
        std::nullopt
    };
}

Status validateCampusId(
    int campusId,
    std::string_view action
    )
{
    if (campusId > 0)
    {
        return {};
    }

    return std::unexpected(error(
        ErrorCode::InvalidArgument,
        std::string(action) + " requires a positive campus id."
        ));
}

Result<int> campusIdFromValue(
    const SqliteValue& value
    )
{
    const auto* id = std::get_if<std::int64_t>(&value);
    if (id == nullptr
        || *id <= 0
        || *id > std::numeric_limits<int>::max())
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an invalid campus id."
            ));
    }

    return static_cast<int>(*id);
}

Result<std::string> textFromValue(
    const SqliteValue& value,
    std::string_view column
    )
{
    if (const auto* text = std::get_if<std::string>(&value); text != nullptr)
    {
        return *text;
    }
    if (std::holds_alternative<std::monostate>(value))
    {
        return std::string{};
    }

    return std::unexpected(error(
        ErrorCode::Schema,
        "SQLite returned a non-text campus "
            + std::string(column) + " value."
        ));
}

Result<CampusRecord> campusFromRow(
    const SqliteRow& row
    )
{
    if (row.values.size() != 15)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an unexpected campus row shape."
            ));
    }

    const Result<int> id = campusIdFromValue(row.values[0]);
    if (!id)
    {
        return std::unexpected(id.error());
    }

    CampusRecord campus;
    campus.id = *id;
    std::string* const fields[] = {
        &campus.name,
        &campus.buildingName,
        &campus.address,
        &campus.phoneNumber,
        &campus.officeNumber,
        &campus.transitSteps,
        &campus.arrivalInfo,
        &campus.imagePath,
        &campus.officeWifi,
        &campus.officeWifiPassword,
        &campus.printerName,
        &campus.printerSteps,
        &campus.photocopierCode,
        &campus.housingLocations
    };
    const std::string_view columnNames[] = {
        "name",
        "building_name",
        "address",
        "phone_number",
        "office_number",
        "transit_steps",
        "arrival_info",
        "image_path",
        "office_wifi",
        "office_wifi_password",
        "printer_name",
        "printer_steps",
        "photocopier_code",
        "housing_locations"
    };

    for (std::size_t index = 0; index < 14; ++index)
    {
        const Result<std::string> value = textFromValue(
            row.values[index + 1],
            columnNames[index]
            );
        if (!value)
        {
            return std::unexpected(value.error());
        }
        *fields[index] = *value;
    }

    return campus;
}

SqliteParameters campusParameters(
    const CampusRecord& campus
    )
{
    return {
        SqliteValue{campus.name},
        SqliteValue{campus.buildingName},
        SqliteValue{campus.address},
        SqliteValue{campus.phoneNumber},
        SqliteValue{campus.officeNumber},
        SqliteValue{campus.transitSteps},
        SqliteValue{campus.arrivalInfo},
        SqliteValue{campus.imagePath},
        SqliteValue{campus.officeWifi},
        SqliteValue{campus.officeWifiPassword},
        SqliteValue{campus.printerName},
        SqliteValue{campus.printerSteps},
        SqliteValue{campus.photocopierCode},
        SqliteValue{campus.housingLocations}
    };
}

Result<bool> campusExists(
    SqliteDatabase& database,
    int campusId
    )
{
    const auto rows = database.query(
        "SELECT EXISTS(SELECT 1 FROM campuses WHERE id=?)",
        SqliteParameters{
            SqliteValue{std::int64_t{campusId}}
        }
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }
    if (rows->rows.size() != 1
        || rows->rows.front().values.size() != 1)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an unexpected campus existence result."
            ));
    }

    const auto* value = std::get_if<std::int64_t>(
        &rows->rows.front().values.front()
        );
    if (value == nullptr)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned a non-integer campus existence result."
            ));
    }

    return *value != 0;
}

Error campusNotFound(
    int campusId
    )
{
    return error(
        ErrorCode::NotFound,
        "No campus exists for id " + std::to_string(campusId) + "."
        );
}

const char* campusColumns()
{
    return "id, name, building_name, address, phone_number, office_number, "
        "transit_steps, arrival_info, image_path, office_wifi, "
        "office_wifi_password, printer_name, printer_steps, "
        "photocopier_code, housing_locations";
}
} // namespace

CampusRecordService::CampusRecordService(
    SqliteDatabase& database
    )
    : m_database(database)
{
}

Result<int> CampusRecordService::create(
    const CampusRecord& campus
    )
{
    const Status inserted = m_database.execute(
        "INSERT INTO campuses ("
        "name, building_name, address, phone_number, office_number, "
        "transit_steps, arrival_info, image_path, office_wifi, "
        "office_wifi_password, printer_name, printer_steps, "
        "photocopier_code, housing_locations"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
        campusParameters(campus)
        );
    if (!inserted)
    {
        return std::unexpected(inserted.error());
    }

    const auto rowId = m_database.query("SELECT last_insert_rowid()");
    if (!rowId)
    {
        return std::unexpected(rowId.error());
    }
    if (rowId->rows.size() != 1
        || rowId->rows.front().values.size() != 1)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite did not return the new campus id."
            ));
    }

    return campusIdFromValue(rowId->rows.front().values.front());
}

Result<int> CampusRecordService::save(
    const CampusRecord& campus
    )
{
    if (campus.id > 0)
    {
        const Status updated = update(campus);
        if (!updated)
        {
            return std::unexpected(updated.error());
        }
        return campus.id;
    }

    return create(campus);
}

Status CampusRecordService::update(
    const CampusRecord& campus
    )
{
    const Status validId = validateCampusId(campus.id, "Updating a campus");
    if (!validId)
    {
        return validId;
    }

    const Result<bool> exists = campusExists(m_database, campus.id);
    if (!exists)
    {
        return std::unexpected(exists.error());
    }
    if (!*exists)
    {
        return std::unexpected(campusNotFound(campus.id));
    }

    SqliteParameters parameters = campusParameters(campus);
    parameters.push_back(SqliteValue{std::int64_t{campus.id}});
    return m_database.execute(
        "UPDATE campuses SET "
        "name=?, building_name=?, address=?, phone_number=?, "
        "office_number=?, transit_steps=?, arrival_info=?, image_path=?, "
        "office_wifi=?, office_wifi_password=?, printer_name=?, "
        "printer_steps=?, photocopier_code=?, housing_locations=? "
        "WHERE id=?",
        parameters
        );
}

Result<CampusRecord> CampusRecordService::get(
    int campusId
    )
{
    const Status validId = validateCampusId(campusId, "Loading a campus");
    if (!validId)
    {
        return std::unexpected(validId.error());
    }

    const auto rows = m_database.query(
        std::string("SELECT ") + campusColumns()
            + " FROM campuses WHERE id=?",
        SqliteParameters{
            SqliteValue{std::int64_t{campusId}}
        }
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }
    if (rows->rows.empty())
    {
        return std::unexpected(campusNotFound(campusId));
    }

    return campusFromRow(rows->rows.front());
}

Result<std::vector<CampusRecord>> CampusRecordService::list()
{
    const auto rows = m_database.query(
        std::string("SELECT ") + campusColumns()
            + " FROM campuses ORDER BY name"
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }

    std::vector<CampusRecord> campuses;
    campuses.reserve(rows->rows.size());
    for (const SqliteRow& row : rows->rows)
    {
        const Result<CampusRecord> campus = campusFromRow(row);
        if (!campus)
        {
            return std::unexpected(campus.error());
        }
        campuses.push_back(*campus);
    }

    return campuses;
}

Status CampusRecordService::remove(
    int campusId
    )
{
    const Status validId = validateCampusId(campusId, "Deleting a campus");
    if (!validId)
    {
        return validId;
    }

    return m_database.execute(
        "DELETE FROM campuses WHERE id=?",
        SqliteParameters{
            SqliteValue{std::int64_t{campusId}}
        }
        );
}

} // namespace classmngr::engine
