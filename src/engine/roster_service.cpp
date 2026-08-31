#include "classmngr/engine/roster_service.h"

#include "classmngr/engine/sqlite_database.h"

#include <cstdint>
#include <limits>
#include <optional>
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
        std::string(action) + " requires a positive class id."
        ));
}

Error classNotFound(
    int classId
    )
{
    return error(
        ErrorCode::NotFound,
        "No class exists for id " + std::to_string(classId) + "."
        );
}

Error withContext(
    Error source,
    std::string_view action,
    std::optional<int> classId = std::nullopt
    )
{
    std::string message(action);
    if (classId)
    {
        message += " for class id ";
        message += std::to_string(*classId);
    }
    if (!source.message.empty())
    {
        message += ": ";
        message += source.message;
    }
    source.message = std::move(message);
    return source;
}

Result<bool> classExists(
    SqliteDatabase& database,
    int classId
    )
{
    const auto rows = database.query(
        "SELECT EXISTS(SELECT 1 FROM classes WHERE id=?)",
        SqliteParameters{SqliteValue{std::int64_t{classId}}}
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
            "SQLite returned an unexpected class existence result."
            ));
    }

    const auto* value = std::get_if<std::int64_t>(
        &rows->rows.front().values.front()
        );
    if (value == nullptr)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned a non-integer class existence result."
            ));
    }

    return *value != 0;
}

Status requireClass(
    SqliteDatabase& database,
    int classId,
    std::string_view action
    )
{
    const Result<bool> present = classExists(database, classId);
    if (!present)
    {
        return std::unexpected(withContext(
            present.error(),
            action,
            classId
            ));
    }
    if (!*present)
    {
        return std::unexpected(withContext(
            classNotFound(classId),
            action,
            classId
            ));
    }

    return {};
}

Result<std::string> textValue(
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
        "SQLite returned a non-text " + std::string(column) + " value."
        ));
}

Result<int> integerValue(
    const SqliteValue& value,
    std::string_view column,
    int nullValue
    )
{
    if (std::holds_alternative<std::monostate>(value))
    {
        return nullValue;
    }

    const auto* integer = std::get_if<std::int64_t>(&value);
    if (integer == nullptr
        || *integer < std::numeric_limits<int>::min()
        || *integer > std::numeric_limits<int>::max())
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an invalid " + std::string(column) + " value."
            ));
    }

    return static_cast<int>(*integer);
}
} // namespace

RosterService::RosterService(
    SqliteDatabase& database
    )
    : m_database(database)
{
}

Result<Roster> RosterService::load(
    int classId
    )
{
    const Status valid = validateClassId(classId, "Loading roster");
    if (!valid)
    {
        return std::unexpected(valid.error());
    }

    const Status present = requireClass(
        m_database,
        classId,
        "Loading roster"
        );
    if (!present)
    {
        return std::unexpected(present.error());
    }

    const auto columnRows = m_database.query(
        "SELECT name, width FROM roster_columns "
        "WHERE class_id=? ORDER BY position, id",
        SqliteParameters{SqliteValue{std::int64_t{classId}}}
        );
    if (!columnRows)
    {
        return std::unexpected(withContext(
            columnRows.error(),
            "Loading roster columns",
            classId
            ));
    }

    Roster roster;
    roster.columns.reserve(columnRows->rows.size());
    roster.columnWidths.reserve(columnRows->rows.size());
    for (const SqliteRow& row : columnRows->rows)
    {
        if (row.values.size() != 2)
        {
            return std::unexpected(withContext(
                error(
                    ErrorCode::Schema,
                    "SQLite returned an unexpected roster column row shape."
                    ),
                "Loading roster columns",
                classId
                ));
        }

        const Result<std::string> name = textValue(row.values[0], "name");
        const Result<int> width = integerValue(row.values[1], "width", 0);
        if (!name)
        {
            return std::unexpected(withContext(
                name.error(),
                "Loading roster columns",
                classId
                ));
        }
        if (!width)
        {
            return std::unexpected(withContext(
                width.error(),
                "Loading roster columns",
                classId
                ));
        }

        roster.columns.push_back(*name);
        roster.columnWidths.push_back(*width);
    }

    if (roster.columns.empty())
    {
        return roster;
    }

    const auto dataRows = m_database.query(
        "SELECT row_index, col_index, value FROM roster_data "
        "WHERE class_id=? ORDER BY row_index, col_index",
        SqliteParameters{SqliteValue{std::int64_t{classId}}}
        );
    if (!dataRows)
    {
        return std::unexpected(withContext(
            dataRows.error(),
            "Loading roster data",
            classId
            ));
    }

    for (const SqliteRow& row : dataRows->rows)
    {
        if (row.values.size() != 3)
        {
            return std::unexpected(withContext(
                error(
                    ErrorCode::Schema,
                    "SQLite returned an unexpected roster data row shape."
                    ),
                "Loading roster data",
                classId
                ));
        }

        const Result<int> rowIndex = integerValue(
            row.values[0],
            "row_index",
            -1
            );
        const Result<int> columnIndex = integerValue(
            row.values[1],
            "col_index",
            -1
            );
        const Result<std::string> value = textValue(row.values[2], "value");
        if (!rowIndex)
        {
            return std::unexpected(withContext(
                rowIndex.error(),
                "Loading roster data",
                classId
                ));
        }
        if (!columnIndex)
        {
            return std::unexpected(withContext(
                columnIndex.error(),
                "Loading roster data",
                classId
                ));
        }
        if (!value)
        {
            return std::unexpected(withContext(
                value.error(),
                "Loading roster data",
                classId
                ));
        }

        if (*rowIndex < 0
            || *columnIndex < 0
            || static_cast<std::size_t>(*columnIndex) >= roster.columns.size())
        {
            continue;
        }

        while (roster.rows.size() <= static_cast<std::size_t>(*rowIndex))
        {
            roster.rows.emplace_back(
                roster.columns.size(),
                std::string{}
                );
        }

        roster.rows[static_cast<std::size_t>(*rowIndex)]
            [static_cast<std::size_t>(*columnIndex)] = *value;
    }

    return roster;
}

Status RosterService::save(
    int classId,
    const Roster& roster
    )
{
    const Status valid = validateClassId(classId, "Saving roster");
    if (!valid)
    {
        return valid;
    }

    const Status present = requireClass(
        m_database,
        classId,
        "Saving roster"
        );
    if (!present)
    {
        return present;
    }

    Result<SqliteTransaction> transactionResult =
        m_database.beginTransaction();
    if (!transactionResult)
    {
        return std::unexpected(withContext(
            transactionResult.error(),
            "Starting roster save transaction",
            classId
            ));
    }
    SqliteTransaction transaction = std::move(*transactionResult);

    const Status contents = saveContents(classId, roster);
    if (!contents)
    {
        return contents;
    }

    const Status committed = transaction.commit();
    if (!committed)
    {
        return std::unexpected(withContext(
            committed.error(),
            "Committing roster save",
            classId
            ));
    }

    return {};
}

Status RosterService::saveBatch(
    const std::vector<std::pair<int, Roster>>& rosters
    )
{
    if (rosters.empty())
    {
        return {};
    }

    for (const auto& [classId, roster] : rosters)
    {
        static_cast<void>(roster);
        const Status valid = validateClassId(classId, "Saving roster");
        if (!valid)
        {
            return valid;
        }

        const Status present = requireClass(
            m_database,
            classId,
            "Saving roster"
            );
        if (!present)
        {
            return present;
        }
    }

    Result<SqliteTransaction> transactionResult =
        m_database.beginTransaction();
    if (!transactionResult)
    {
        return std::unexpected(withContext(
            transactionResult.error(),
            "Starting roster batch save transaction"
            ));
    }
    SqliteTransaction transaction = std::move(*transactionResult);

    for (const auto& [classId, roster] : rosters)
    {
        const Status contents = saveContents(classId, roster);
        if (!contents)
        {
            return contents;
        }
    }

    const Status committed = transaction.commit();
    if (!committed)
    {
        return std::unexpected(withContext(
            committed.error(),
            "Committing roster batch save"
            ));
    }

    return {};
}

Status RosterService::saveContents(
    int classId,
    const Roster& roster
    )
{
    Status statement = m_database.execute(
        "DELETE FROM roster_columns WHERE class_id=?",
        SqliteParameters{SqliteValue{std::int64_t{classId}}}
        );
    if (!statement)
    {
        return std::unexpected(withContext(
            statement.error(),
            "Deleting roster columns",
            classId
            ));
    }

    statement = m_database.execute(
        "DELETE FROM roster_data WHERE class_id=?",
        SqliteParameters{SqliteValue{std::int64_t{classId}}}
        );
    if (!statement)
    {
        return std::unexpected(withContext(
            statement.error(),
            "Deleting roster data",
            classId
            ));
    }

    for (std::size_t column = 0;
         column < roster.columns.size();
         ++column)
    {
        const int width = column < roster.columnWidths.size()
            ? roster.columnWidths[column]
            : 0;
        statement = m_database.execute(
            "INSERT INTO roster_columns (class_id, name, position, width) "
            "VALUES (?, ?, ?, ?)",
            SqliteParameters{
                SqliteValue{std::int64_t{classId}},
                SqliteValue{roster.columns[column]},
                SqliteValue{std::int64_t{static_cast<std::int64_t>(column)}},
                SqliteValue{std::int64_t{width}}
            }
            );
        if (!statement)
        {
            return std::unexpected(withContext(
                statement.error(),
                "Inserting roster column",
                classId
                ));
        }
    }

    for (std::size_t row = 0; row < roster.rows.size(); ++row)
    {
        const std::vector<std::string>& rowValues = roster.rows[row];
        for (std::size_t column = 0;
             column < roster.columns.size();
             ++column)
        {
            if (column >= rowValues.size()
                || rowValues[column].empty())
            {
                continue;
            }

            statement = m_database.execute(
                "INSERT INTO roster_data "
                "(class_id, row_index, col_index, value) "
                "VALUES (?, ?, ?, ?)",
                SqliteParameters{
                    SqliteValue{std::int64_t{classId}},
                    SqliteValue{
                        std::int64_t{static_cast<std::int64_t>(row)}},
                    SqliteValue{
                        std::int64_t{static_cast<std::int64_t>(column)}},
                    SqliteValue{rowValues[column]}
                }
                );
            if (!statement)
            {
                return std::unexpected(withContext(
                    statement.error(),
                    "Inserting roster data",
                    classId
                    ));
            }
        }
    }

    return {};
}

} // namespace classmngr::engine
