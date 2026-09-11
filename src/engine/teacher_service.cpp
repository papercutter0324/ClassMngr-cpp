#include "classmngr/engine/teacher_service.h"

#include "classmngr/engine/sqlite_database.h"
#include "classmngr/engine/teacher_validator.h"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

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

Status validateTeacherId(
    int teacherId,
    std::string_view action
    )
{
    if (teacherId > 0)
    {
        return {};
    }

    return std::unexpected(error(
        ErrorCode::InvalidArgument,
        std::string(action) + " requires a positive teacher id."
        ));
}

std::string validationMessage(
    const ValidationResult& validation
    )
{
    std::string message = "Teacher validation failed";
    bool first = true;
    for (const ValidationIssue& issue : validation.issues())
    {
        if (!issue.isError())
        {
            continue;
        }

        message += first ? ": " : "; ";
        first = false;
        message += issue.code;
        if (!issue.field.empty())
        {
            message += " (";
            message += issue.field;
            message += ')';
        }
    }

    message += '.';
    return message;
}

Result<Teacher> normalizeAndValidate(
    const Teacher& source
    )
{
    Teacher normalized = TeacherValidator::normalized(source);
    const ValidationResult validation = TeacherValidator::validate(normalized);
    if (validation.hasErrors())
    {
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            validationMessage(validation)
            ));
    }

    return normalized;
}

Result<int> teacherIdFromValue(
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
            "SQLite returned an invalid teacher id."
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
        "SQLite returned a non-text teacher "
        + std::string(column) + " value."
        ));
}

Result<Teacher> teacherFromRow(
    const SqliteRow& row
    )
{
    if (row.values.size() != 15)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an unexpected teacher row shape."
            ));
    }

    const Result<int> id = teacherIdFromValue(row.values[0]);
    if (!id)
    {
        return std::unexpected(id.error());
    }

    Teacher teacher;
    teacher.id = *id;
    std::string* const fields[] = {
        &teacher.teacherKr,
        &teacher.teacherEn,
        &teacher.preferredRomanization,
        &teacher.preferredName,
        &teacher.roomNumber,
        &teacher.birthday,
        &teacher.phoneNumber,
        &teacher.wifiName,
        &teacher.wifiPassword,
        &teacher.internetType,
        &teacher.zoomId,
        &teacher.zoomPassword,
        &teacher.projectionType,
        &teacher.notes
    };
    const std::string_view columnNames[] = {
        "teacher_kr",
        "teacher_en",
        "preferred_romanization",
        "preferred_name",
        "room_number",
        "birthday",
        "phone_number",
        "wifi_name",
        "wifi_password",
        "internet_type",
        "zoom_id",
        "zoom_password",
        "projection_type",
        "notes"
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

    return teacher;
}

SqliteParameters teacherParameters(
    const Teacher& teacher
    )
{
    return {
        SqliteValue{teacher.teacherKr},
        SqliteValue{teacher.teacherEn},
        SqliteValue{teacher.preferredRomanization},
        SqliteValue{teacher.preferredName},
        SqliteValue{teacher.roomNumber},
        SqliteValue{teacher.birthday},
        SqliteValue{teacher.phoneNumber},
        SqliteValue{teacher.wifiName},
        SqliteValue{teacher.wifiPassword},
        SqliteValue{teacher.internetType},
        SqliteValue{teacher.zoomId},
        SqliteValue{teacher.zoomPassword},
        SqliteValue{teacher.projectionType},
        SqliteValue{teacher.notes}
    };
}

Result<bool> teacherExists(
    SqliteDatabase& database,
    int teacherId
    )
{
    const auto rows = database.query(
        "SELECT EXISTS(SELECT 1 FROM teachers WHERE id=?)",
        SqliteParameters{
            SqliteValue{std::int64_t{teacherId}}
        }
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }
    if (rows->rows.size() != 1 || rows->rows.front().values.size() != 1)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an unexpected teacher existence result."
            ));
    }

    const auto* value = std::get_if<std::int64_t>(
        &rows->rows.front().values.front()
        );
    if (value == nullptr)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned a non-integer teacher existence result."
            ));
    }

    return *value != 0;
}

Error teacherNotFound(
    int teacherId
    )
{
    return error(
        ErrorCode::NotFound,
        "No teacher exists for id " + std::to_string(teacherId) + "."
        );
}

const char* teacherColumns()
{
    return "id, teacher_kr, teacher_en, preferred_romanization, "
        "preferred_name, room_number, birthday, phone_number, wifi_name, "
        "wifi_password, internet_type, zoom_id, zoom_password, "
        "projection_type, notes";
}
} // namespace

TeacherService::TeacherService(
    SqliteDatabase& database
    )
    : m_database(database)
{
}

Result<int> TeacherService::create(
    const Teacher& teacher
    )
{
    const Result<Teacher> normalized = normalizeAndValidate(teacher);
    if (!normalized)
    {
        return std::unexpected(normalized.error());
    }

    const Status inserted = m_database.execute(
        "INSERT INTO teachers ("
        "teacher_kr, teacher_en, preferred_romanization, preferred_name, "
        "room_number, birthday, phone_number, wifi_name, wifi_password, "
        "internet_type, zoom_id, zoom_password, projection_type, notes"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
        teacherParameters(*normalized)
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
    if (rowId->rows.size() != 1 || rowId->rows.front().values.size() != 1)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite did not return the new teacher id."
            ));
    }

    return teacherIdFromValue(rowId->rows.front().values.front());
}

Result<int> TeacherService::save(
    const Teacher& teacher
    )
{
    if (teacher.id > 0)
    {
        const Status updated = update(teacher);
        if (!updated)
        {
            return std::unexpected(updated.error());
        }
        return teacher.id;
    }

    return create(teacher);
}

Status TeacherService::update(
    const Teacher& teacher
    )
{
    const Status validId = validateTeacherId(teacher.id, "Updating a teacher");
    if (!validId)
    {
        return validId;
    }

    const Result<Teacher> normalized = normalizeAndValidate(teacher);
    if (!normalized)
    {
        return std::unexpected(normalized.error());
    }

    const Result<bool> exists = teacherExists(m_database, teacher.id);
    if (!exists)
    {
        return std::unexpected(exists.error());
    }
    if (!*exists)
    {
        return std::unexpected(teacherNotFound(teacher.id));
    }

    SqliteParameters parameters = teacherParameters(*normalized);
    parameters.push_back(SqliteValue{std::int64_t{teacher.id}});
    return m_database.execute(
        "UPDATE teachers SET "
        "teacher_kr=?, teacher_en=?, preferred_romanization=?, "
        "preferred_name=?, room_number=?, birthday=?, phone_number=?, "
        "wifi_name=?, wifi_password=?, internet_type=?, zoom_id=?, "
        "zoom_password=?, projection_type=?, notes=? WHERE id=?",
        parameters
        );
}

Result<Teacher> TeacherService::get(
    int teacherId
    )
{
    const Status validId = validateTeacherId(teacherId, "Loading a teacher");
    if (!validId)
    {
        return std::unexpected(validId.error());
    }

    const auto rows = m_database.query(
        std::string("SELECT ") + teacherColumns()
            + " FROM teachers WHERE id=?",
        SqliteParameters{
            SqliteValue{std::int64_t{teacherId}}
        }
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }
    if (rows->rows.empty())
    {
        return std::unexpected(teacherNotFound(teacherId));
    }

    return teacherFromRow(rows->rows.front());
}

Result<std::vector<Teacher>> TeacherService::list()
{
    const auto rows = m_database.query(
        std::string("SELECT ") + teacherColumns()
            + " FROM teachers ORDER BY teacher_en"
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }

    std::vector<Teacher> teachers;
    teachers.reserve(rows->rows.size());
    for (const SqliteRow& row : rows->rows)
    {
        const Result<Teacher> teacher = teacherFromRow(row);
        if (!teacher)
        {
            return std::unexpected(teacher.error());
        }
        teachers.push_back(*teacher);
    }

    return teachers;
}

Status TeacherService::remove(
    int teacherId
    )
{
    const Status validId = validateTeacherId(teacherId, "Deleting a teacher");
    if (!validId)
    {
        return validId;
    }

    Result<SqliteTransaction> transaction = m_database.beginTransaction();
    if (!transaction)
    {
        return std::unexpected(transaction.error());
    }

    const Result<bool> exists = teacherExists(m_database, teacherId);
    if (!exists)
    {
        return std::unexpected(exists.error());
    }
    if (!*exists)
    {
        return std::unexpected(teacherNotFound(teacherId));
    }

    const Status clearAssignments = m_database.execute(
        "UPDATE class_info SET teacher_id=NULL WHERE teacher_id=?",
        SqliteParameters{
            SqliteValue{std::int64_t{teacherId}}
        }
        );
    if (!clearAssignments)
    {
        return clearAssignments;
    }

    const Status deleted = m_database.execute(
        "DELETE FROM teachers WHERE id=?",
        SqliteParameters{
            SqliteValue{std::int64_t{teacherId}}
        }
        );
    if (!deleted)
    {
        return deleted;
    }

    return transaction->commit();
}

} // namespace classmngr::engine
