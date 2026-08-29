#include "classmngr/engine/class_repository.h"

#include "classmngr/engine/sqlite_database.h"

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

Result<int> classIdFromValue(
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
            "SQLite returned an invalid class id."
            ));
    }

    return static_cast<int>(*id);
}

Result<std::string> classNameFromValue(
    const SqliteValue& value
    )
{
    if (const auto* name = std::get_if<std::string>(&value); name != nullptr)
    {
        return *name;
    }
    if (std::holds_alternative<std::monostate>(value))
    {
        return std::string{};
    }

    return std::unexpected(error(
        ErrorCode::Schema,
        "SQLite returned a non-text class name."
        ));
}

Result<Classroom> classroomFromRow(
    const SqliteRow& row
    )
{
    if (row.values.size() != 2)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an unexpected class row shape."
            ));
    }

    const Result<int> id = classIdFromValue(row.values[0]);
    if (!id)
    {
        return std::unexpected(id.error());
    }
    const Result<std::string> name = classNameFromValue(row.values[1]);
    if (!name)
    {
        return std::unexpected(name.error());
    }

    return Classroom{*id, *name};
}

Status validateClassId(
    int classId,
    std::string_view action
    )
{
    if (classId > 0)
    {
        return {};
    }

    return std::unexpected(error(
        ErrorCode::InvalidArgument,
        std::string(action)
        + " requires a positive class id."
        ));
}
} // namespace

ClassRepository::ClassRepository(
    SqliteDatabase& database
    )
    : m_database(database)
{
}

Result<int> ClassRepository::create(
    std::string_view name
    )
{
    const Status inserted = m_database.execute(
        "INSERT INTO classes (name) VALUES (?)",
        SqliteParameters{
            SqliteValue{std::string(name)}
        }
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
            "SQLite did not return the new class id."
            ));
    }

    return classIdFromValue(rowId->rows.front().values.front());
}

Result<std::vector<Classroom>> ClassRepository::list()
{
    const auto rows = m_database.query(
        "SELECT c.id, c.name "
        "FROM classes c "
        "LEFT JOIN testing_classes tc ON tc.class_id = c.id "
        "WHERE tc.class_id IS NULL "
        "ORDER BY c.name"
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }

    std::vector<Classroom> classes;
    classes.reserve(rows->rows.size());
    for (const SqliteRow& row : rows->rows)
    {
        const Result<Classroom> classroom = classroomFromRow(row);
        if (!classroom)
        {
            return std::unexpected(classroom.error());
        }
        classes.push_back(*classroom);
    }

    return classes;
}

Result<Classroom> ClassRepository::get(
    int classId
    )
{
    const Status valid = validateClassId(classId, "Loading a class");
    if (!valid)
    {
        return std::unexpected(valid.error());
    }

    const auto rows = m_database.query(
        "SELECT id, name FROM classes WHERE id=?",
        SqliteParameters{
            SqliteValue{std::int64_t{classId}}
        }
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }
    if (rows->rows.empty())
    {
        return std::unexpected(error(
            ErrorCode::NotFound,
            "No class exists for id " + std::to_string(classId) + "."
            ));
    }

    return classroomFromRow(rows->rows.front());
}

Status ClassRepository::rename(
    int classId,
    std::string_view name
    )
{
    const Status valid = validateClassId(classId, "Renaming a class");
    if (!valid)
    {
        return valid;
    }

    return m_database.execute(
        "UPDATE classes SET name=? WHERE id=?",
        SqliteParameters{
            SqliteValue{std::string(name)},
            SqliteValue{std::int64_t{classId}}
        }
        );
}

Status ClassRepository::remove(
    int classId
    )
{
    const Status valid = validateClassId(classId, "Deleting a class");
    if (!valid)
    {
        return valid;
    }

    return m_database.execute(
        "DELETE FROM classes WHERE id=?",
        SqliteParameters{
            SqliteValue{std::int64_t{classId}}
        }
        );
}

} // namespace classmngr::engine
