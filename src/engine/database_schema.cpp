#include "classmngr/engine/database_schema.h"

#include "classmngr/engine/sqlite_database.h"

#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace classmngr::engine
{
namespace
{
struct SchemaStatement
{
    std::string_view sql;
    std::string_view objectType;
    std::string_view objectName;
};

struct SchemaColumn
{
    std::string_view tableName;
    std::string_view columnName;
    std::string_view definition;
};

struct SchemaMigration
{
    int version;
    std::string_view name;
    Status (*apply)(SqliteDatabase& database);
};

struct ConstrainedSchemaTable
{
    std::string_view tableName;
    std::string_view sql;
    std::string_view columnNames;
};

struct LegacyPreflightCheck
{
    std::string_view description;
    std::string_view countQuery;
};

Error makeError(
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

Error migrationError(
    const SchemaMigration& migration,
    const Error& cause
    )
{
    Error error = cause;
    error.code = ErrorCode::Migration;
    error.message =
        "Database migration "
        + std::to_string(migration.version)
        + " ("
        + std::string(migration.name)
        + ") failed: "
        + cause.message;
    return error;
}

std::string quoteIdentifier(
    std::string_view identifier
    )
{
    std::string result;
    result.reserve(identifier.size() + 2);
    result += '"';
    for (const char character : identifier)
    {
        result += character;
        if (character == '"')
        {
            result += '"';
        }
    }
    result += '"';
    return result;
}

std::string quoteSqlLiteral(
    std::string_view value
    )
{
    std::string result;
    result.reserve(value.size() + 2);
    result += '\'';
    for (const char character : value)
    {
        result += character;
        if (character == '\'')
        {
            result += '\'';
        }
    }
    result += '\'';
    return result;
}

const std::string* stringValue(
    const SqliteValue& value
    ) noexcept
{
    return std::get_if<std::string>(&value);
}

std::string valueAsString(
    const SqliteValue& value
    )
{
    if (const auto* text = stringValue(value); text != nullptr)
    {
        return *text;
    }
    if (const auto* integer = std::get_if<std::int64_t>(&value);
        integer != nullptr)
    {
        return std::to_string(*integer);
    }
    if (const auto* real = std::get_if<double>(&value); real != nullptr)
    {
        return std::to_string(*real);
    }
    if (std::holds_alternative<std::monostate>(value))
    {
        return "<null>";
    }
    return "<blob>";
}

Result<std::int64_t> firstInteger(
    const SqliteQueryResult& result,
    std::string_view description
    )
{
    if (result.rows.size() != 1 || result.rows.front().values.size() != 1)
    {
        return std::unexpected(makeError(
            ErrorCode::Schema,
            "SQLite did not return exactly one value for "
            + std::string(description)
            + "."
            ));
    }

    const auto* value = std::get_if<std::int64_t>(
        &result.rows.front().values.front()
        );
    if (value == nullptr)
    {
        return std::unexpected(makeError(
            ErrorCode::Schema,
            "SQLite returned a non-integer value for "
            + std::string(description)
            + "."
            ));
    }

    return *value;
}

Status executeSchemaStatement(
    SqliteDatabase& database,
    const SchemaStatement& statement
    )
{
    const Status executed = database.execute(statement.sql);
    if (!executed)
    {
        return executed;
    }

    const auto verified = database.query(
        "SELECT type FROM sqlite_master WHERE name=?",
        SqliteParameters{
            SqliteValue{std::string(statement.objectName)}
        }
        );
    if (!verified)
    {
        return std::unexpected(verified.error());
    }
    if (verified->rows.size() != 1
        || verified->rows.front().values.size() != 1)
    {
        return std::unexpected(makeError(
            ErrorCode::Schema,
            "Creating database "
            + std::string(statement.objectType)
            + " '"
            + std::string(statement.objectName)
            + "' did not create the expected object."
            ));
    }

    const auto* actualType = stringValue(
        verified->rows.front().values.front()
        );
    if (actualType == nullptr || *actualType != statement.objectType)
    {
        return std::unexpected(makeError(
            ErrorCode::Schema,
            "Expected database "
            + std::string(statement.objectType)
            + " '"
            + std::string(statement.objectName)
            + "', but an existing "
            + (actualType == nullptr ? "object" : *actualType)
            + " has that name."
            ));
    }

    return {};
}

Result<bool> tableHasColumn(
    SqliteDatabase& database,
    std::string_view tableName,
    std::string_view columnName
    )
{
    const std::string queryText =
        "PRAGMA table_info(" + quoteIdentifier(tableName) + ")";
    const auto result = database.query(queryText);
    if (!result)
    {
        return std::unexpected(result.error());
    }

    for (const SqliteRow& row : result->rows)
    {
        if (row.values.size() > 1)
        {
            const auto* actualName = stringValue(row.values[1]);
            if (actualName != nullptr && *actualName == columnName)
            {
                return true;
            }
        }
    }

    return false;
}

Status ensureTableColumn(
    SqliteDatabase& database,
    const SchemaColumn& column
    )
{
    const Result<bool> hasColumn = tableHasColumn(
        database,
        column.tableName,
        column.columnName
        );
    if (!hasColumn)
    {
        return std::unexpected(hasColumn.error());
    }
    if (*hasColumn)
    {
        return {};
    }

    const std::string queryText =
        "ALTER TABLE "
        + quoteIdentifier(column.tableName)
        + " ADD COLUMN "
        + quoteIdentifier(column.columnName)
        + " "
        + std::string(column.definition);
    return database.execute(queryText);
}

const SchemaStatement SchemaStatements[] = {
    { R"SQL(
        CREATE TABLE IF NOT EXISTS app_settings (
            key TEXT PRIMARY KEY,
            value TEXT
        )
    )SQL", "table", "app_settings" },
    { R"SQL(
        CREATE TABLE IF NOT EXISTS roster_columns (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            class_id INTEGER,
            name TEXT,
            position INTEGER,
            width INTEGER
        )
    )SQL", "table", "roster_columns" },
    { R"SQL(
        CREATE TABLE IF NOT EXISTS roster_data (
            class_id INTEGER,
            row_index INTEGER,
            col_index INTEGER,
            value TEXT,
            PRIMARY KEY (class_id, row_index, col_index)
        )
    )SQL", "table", "roster_data" },
    { R"SQL(
        CREATE TABLE IF NOT EXISTS speaking_evaluations (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            class_id INTEGER,
            evaluation_name TEXT,
            UNIQUE(class_id, evaluation_name)
        )
    )SQL", "table", "speaking_evaluations" },
    { R"SQL(
        CREATE TABLE IF NOT EXISTS speaking_eval_data (
            evaluation_id INTEGER,
            row_index INTEGER,
            col_0 TEXT,
            col_1 TEXT,
            col_2 TEXT,
            col_3 TEXT,
            col_4 TEXT,
            col_5 TEXT,
            col_6 TEXT,
            col_7 TEXT,
            col_8 TEXT,
            col_9 TEXT,
            col_10 TEXT,
            PRIMARY KEY (evaluation_id, row_index)
        )
    )SQL", "table", "speaking_eval_data" },
    { R"SQL(
        CREATE TABLE IF NOT EXISTS campuses (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            building_name TEXT,
            address TEXT,
            phone_number TEXT,
            office_number TEXT,
            transit_steps TEXT,
            arrival_info TEXT,
            image_path TEXT,
            office_wifi TEXT,
            office_wifi_password TEXT,
            printer_name TEXT,
            printer_steps TEXT,
            photocopier_code TEXT,
            housing_locations TEXT
        )
    )SQL", "table", "campuses" },
    { R"SQL(
        CREATE TABLE IF NOT EXISTS teachers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            teacher_kr TEXT,
            teacher_en TEXT,
            preferred_romanization TEXT,
            preferred_name TEXT,
            room_number TEXT,
            birthday TEXT,
            phone_number TEXT,
            wifi_name TEXT,
            wifi_password TEXT,
            internet_type TEXT DEFAULT 'WiFi',
            zoom_id TEXT,
            zoom_password TEXT,
            projection_type TEXT DEFAULT 'HDMI',
            notes TEXT
        )
    )SQL", "table", "teachers" },
    { R"SQL(
        CREATE TABLE IF NOT EXISTS native_english_teachers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT,
            position TEXT,
            phone_number TEXT,
            birthday TEXT,
            nationality TEXT,
            email TEXT
        )
    )SQL", "table", "native_english_teachers" },
    { R"SQL(
        CREATE TABLE IF NOT EXISTS gs_team (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT,
            korean_name TEXT,
            position TEXT,
            phone_number TEXT,
            birthday TEXT
        )
    )SQL", "table", "gs_team" },
    { R"SQL(
        CREATE TABLE IF NOT EXISTS classes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT
        )
    )SQL", "table", "classes" },
    { R"SQL(
        CREATE TABLE IF NOT EXISTS class_info (
            class_id INTEGER PRIMARY KEY,
            teacher_id INTEGER,
            class_grade TEXT,
            class_level TEXT,
            reading_book TEXT,
            essay_book TEXT,
            class_color TEXT DEFAULT '#FFFFFF',
            font_color TEXT DEFAULT '#000000',
            notes TEXT,
            time_filler_activities TEXT
        )
    )SQL", "table", "class_info" },
    { R"SQL(
        CREATE TABLE IF NOT EXISTS testing_classes (
            class_id INTEGER PRIMARY KEY,
            room TEXT NOT NULL
        )
    )SQL", "table", "testing_classes" },
    { R"SQL(
        CREATE TABLE IF NOT EXISTS class_times (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            class_id INTEGER,
            day TEXT,
            start_time TEXT,
            end_time TEXT
        )
    )SQL", "table", "class_times" },
    { R"SQL(
        CREATE TABLE IF NOT EXISTS class_intensive_times (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            class_id INTEGER,
            day TEXT,
            start_time TEXT,
            end_time TEXT,
            UNIQUE(day, start_time)
        )
    )SQL", "table", "class_intensive_times" },
    { R"SQL(
        CREATE TABLE IF NOT EXISTS intensive_slot_states (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            day TEXT,
            start_time TEXT,
            state TEXT,
            UNIQUE(day, start_time)
        )
    )SQL", "table", "intensive_slot_states" },
    { R"SQL(
        CREATE TABLE IF NOT EXISTS schedule_testing_blocks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            day TEXT NOT NULL,
            start_time TEXT NOT NULL,
            room TEXT NOT NULL DEFAULT '',
            class_id INTEGER,
            UNIQUE(day, start_time)
        )
    )SQL", "table", "schedule_testing_blocks" },
    { R"SQL(
        CREATE TABLE IF NOT EXISTS calendar_events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL,
            event_type TEXT DEFAULT 'Other',
            time_status TEXT DEFAULT 'Timed',
            repeat_series_id TEXT,
            all_day INTEGER DEFAULT 0,
            start_date TEXT,
            start_time TEXT,
            end_date TEXT,
            end_time TEXT
        )
    )SQL", "table", "calendar_events" }
};

const SchemaStatement SchemaIndexStatements[] = {
    { R"SQL(
        CREATE INDEX IF NOT EXISTS idx_schedule_testing_blocks_class_id
        ON schedule_testing_blocks (class_id)
    )SQL", "index", "idx_schedule_testing_blocks_class_id" },
    { R"SQL(
        CREATE INDEX IF NOT EXISTS idx_calendar_events_dates
        ON calendar_events (start_date, end_date, start_time, title)
    )SQL", "index", "idx_calendar_events_dates" },
    { R"SQL(
        CREATE INDEX IF NOT EXISTS idx_calendar_events_end_dates
        ON calendar_events (end_date, start_date, start_time, title)
    )SQL", "index", "idx_calendar_events_end_dates" },
    { R"SQL(
        CREATE INDEX IF NOT EXISTS idx_calendar_events_repeat_series
        ON calendar_events (repeat_series_id, start_date, id)
    )SQL", "index", "idx_calendar_events_repeat_series" }
};

const SchemaColumn SchemaColumns[] = {
    { "campuses", "building_name", "TEXT" },
    { "campuses", "address", "TEXT" },
    { "campuses", "phone_number", "TEXT" },
    { "campuses", "office_number", "TEXT" },
    { "campuses", "transit_steps", "TEXT" },
    { "campuses", "arrival_info", "TEXT" },
    { "campuses", "image_path", "TEXT" },
    { "campuses", "office_wifi", "TEXT" },
    { "campuses", "office_wifi_password", "TEXT" },
    { "campuses", "printer_name", "TEXT" },
    { "campuses", "printer_steps", "TEXT" },
    { "campuses", "photocopier_code", "TEXT" },
    { "campuses", "housing_locations", "TEXT" },
    { "teachers", "preferred_romanization", "TEXT" },
    { "teachers", "preferred_name", "TEXT" },
    { "teachers", "birthday", "TEXT" },
    { "teachers", "phone_number", "TEXT" },
    { "teachers", "internet_type", "TEXT DEFAULT 'WiFi'" },
    { "teachers", "projection_type", "TEXT DEFAULT 'HDMI'" },
    { "native_english_teachers", "email", "TEXT" },
    { "class_info", "time_filler_activities", "TEXT" },
    { "schedule_testing_blocks", "class_id", "INTEGER" },
    { "calendar_events", "event_type", "TEXT DEFAULT 'Other'" },
    { "calendar_events", "time_status", "TEXT DEFAULT 'Timed'" },
    { "calendar_events", "all_day", "INTEGER DEFAULT 0" },
    { "calendar_events", "repeat_series_id", "TEXT" }
};

const ConstrainedSchemaTable ConstrainedSchemaTables[] = {
    {
        "teachers",
        R"SQL(
            CREATE TABLE IF NOT EXISTS teachers (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                teacher_kr TEXT,
                teacher_en TEXT,
                preferred_romanization TEXT,
                preferred_name TEXT,
                room_number TEXT,
                birthday TEXT,
                phone_number TEXT,
                wifi_name TEXT,
                wifi_password TEXT,
                internet_type TEXT NOT NULL DEFAULT 'WiFi'
                    CHECK(internet_type IN ('WiFi', 'LAN', 'Both', 'N/A')),
                zoom_id TEXT,
                zoom_password TEXT,
                projection_type TEXT NOT NULL DEFAULT 'HDMI'
                    CHECK(projection_type IN ('HDMI', 'Zoom', 'Any', 'N/A')),
                notes TEXT
            )
        )SQL",
        "id, teacher_kr, teacher_en, preferred_romanization, preferred_name, "
        "room_number, birthday, phone_number, wifi_name, wifi_password, "
        "internet_type, zoom_id, zoom_password, projection_type, notes"
    },
    {
        "classes",
        R"SQL(
            CREATE TABLE IF NOT EXISTS classes (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT
            )
        )SQL",
        "id, name"
    },
    {
        "class_info",
        R"SQL(
            CREATE TABLE IF NOT EXISTS class_info (
                class_id INTEGER PRIMARY KEY
                    REFERENCES classes(id) ON DELETE CASCADE,
                teacher_id INTEGER
                    REFERENCES teachers(id) ON DELETE SET NULL,
                class_grade TEXT,
                class_level TEXT,
                reading_book TEXT,
                essay_book TEXT,
                class_color TEXT DEFAULT '#FFFFFF',
                font_color TEXT DEFAULT '#000000',
                notes TEXT,
                time_filler_activities TEXT
            )
        )SQL",
        "class_id, teacher_id, class_grade, class_level, reading_book, "
        "essay_book, class_color, font_color, notes, time_filler_activities"
    },
    {
        "testing_classes",
        R"SQL(
            CREATE TABLE IF NOT EXISTS testing_classes (
                class_id INTEGER PRIMARY KEY
                    REFERENCES classes(id) ON DELETE CASCADE,
                room TEXT NOT NULL
            )
        )SQL",
        "class_id, room"
    },
    {
        "class_times",
        R"SQL(
            CREATE TABLE IF NOT EXISTS class_times (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                class_id INTEGER NOT NULL
                    REFERENCES classes(id) ON DELETE CASCADE,
                day TEXT,
                start_time TEXT,
                end_time TEXT
            )
        )SQL",
        "id, class_id, day, start_time, end_time"
    },
    {
        "class_intensive_times",
        R"SQL(
            CREATE TABLE IF NOT EXISTS class_intensive_times (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                class_id INTEGER NOT NULL
                    REFERENCES classes(id) ON DELETE CASCADE,
                day TEXT,
                start_time TEXT,
                end_time TEXT,
                UNIQUE(day, start_time)
            )
        )SQL",
        "id, class_id, day, start_time, end_time"
    },
    {
        "roster_columns",
        R"SQL(
            CREATE TABLE IF NOT EXISTS roster_columns (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                class_id INTEGER NOT NULL
                    REFERENCES classes(id) ON DELETE CASCADE,
                name TEXT,
                position INTEGER NOT NULL CHECK(position >= 0),
                width INTEGER
            )
        )SQL",
        "id, class_id, name, position, width"
    },
    {
        "roster_data",
        R"SQL(
            CREATE TABLE IF NOT EXISTS roster_data (
                class_id INTEGER NOT NULL
                    REFERENCES classes(id) ON DELETE CASCADE,
                row_index INTEGER NOT NULL CHECK(row_index >= 0),
                col_index INTEGER NOT NULL CHECK(col_index >= 0),
                value TEXT,
                PRIMARY KEY (class_id, row_index, col_index)
            )
        )SQL",
        "class_id, row_index, col_index, value"
    },
    {
        "speaking_evaluations",
        R"SQL(
            CREATE TABLE IF NOT EXISTS speaking_evaluations (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                class_id INTEGER NOT NULL
                    REFERENCES classes(id) ON DELETE CASCADE,
                evaluation_name TEXT,
                UNIQUE(class_id, evaluation_name)
            )
        )SQL",
        "id, class_id, evaluation_name"
    },
    {
        "speaking_eval_data",
        R"SQL(
            CREATE TABLE IF NOT EXISTS speaking_eval_data (
                evaluation_id INTEGER NOT NULL
                    REFERENCES speaking_evaluations(id) ON DELETE CASCADE,
                row_index INTEGER NOT NULL CHECK(row_index >= 0),
                col_0 TEXT,
                col_1 TEXT,
                col_2 TEXT,
                col_3 TEXT,
                col_4 TEXT,
                col_5 TEXT,
                col_6 TEXT,
                col_7 TEXT,
                col_8 TEXT,
                col_9 TEXT,
                col_10 TEXT,
                PRIMARY KEY (evaluation_id, row_index)
            )
        )SQL",
        "evaluation_id, row_index, col_0, col_1, col_2, col_3, col_4, "
        "col_5, col_6, col_7, col_8, col_9, col_10"
    },
    {
        "schedule_testing_blocks",
        R"SQL(
            CREATE TABLE IF NOT EXISTS schedule_testing_blocks (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                day TEXT NOT NULL,
                start_time TEXT NOT NULL,
                room TEXT NOT NULL DEFAULT '',
                class_id INTEGER
                    REFERENCES classes(id) ON DELETE SET NULL,
                UNIQUE(day, start_time)
            )
        )SQL",
        "id, day, start_time, room, class_id"
    },
    {
        "calendar_events",
        R"SQL(
            CREATE TABLE IF NOT EXISTS calendar_events (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                title TEXT NOT NULL,
                event_type TEXT NOT NULL DEFAULT 'Other'
                    CHECK(event_type IN (
                        'Vacation', 'Holiday', 'Workshop', 'CM', 'Meeting', 'Other'
                    )),
                time_status TEXT NOT NULL DEFAULT 'Timed'
                    CHECK(time_status IN ('Timed', 'Unknown', 'Unconfirmed')),
                repeat_series_id TEXT,
                all_day INTEGER NOT NULL DEFAULT 0 CHECK(all_day IN (0, 1)),
                start_date TEXT,
                start_time TEXT,
                end_date TEXT,
                end_time TEXT
            )
        )SQL",
        "id, title, event_type, time_status, repeat_series_id, all_day, "
        "start_date, start_time, end_date, end_time"
    }
};

Status setForeignKeyEnforcement(
    SqliteDatabase& database,
    bool enabled
    )
{
    const Status configured = database.execute(
        enabled
            ? "PRAGMA foreign_keys = ON"
            : "PRAGMA foreign_keys = OFF"
        );
    if (!configured)
    {
        return configured;
    }

    const Result<bool> actual = database.foreignKeysEnabled();
    if (!actual)
    {
        return std::unexpected(actual.error());
    }
    if (*actual != enabled)
    {
        return std::unexpected(makeError(
            ErrorCode::Constraint,
            std::string("SQLite did not ")
            + (enabled ? "enable" : "disable")
            + " foreign-key enforcement for this connection."
            ));
    }

    return {};
}

Status verifyForeignKeyIntegrity(
    SqliteDatabase& database
    )
{
    const auto result = database.query("PRAGMA foreign_key_check");
    if (!result)
    {
        return std::unexpected(result.error());
    }
    if (result->rows.empty())
    {
        return {};
    }

    const SqliteRow& row = result->rows.front();
    const std::string table = row.values.size() > 0
        ? valueAsString(row.values[0])
        : "<unknown>";
    const std::string rowId = row.values.size() > 1
        ? valueAsString(row.values[1])
        : "<unknown>";
    const std::string parent = row.values.size() > 2
        ? valueAsString(row.values[2])
        : "<unknown>";
    return std::unexpected(makeError(
        ErrorCode::Constraint,
        "Foreign-key integrity check failed for table '"
        + table
        + "', row "
        + rowId
        + ", referencing parent table '"
        + parent
        + "'."
        ));
}

Status createInitialSchema(
    SqliteDatabase& database
    )
{
    for (const SchemaStatement& statement : SchemaStatements)
    {
        const Status status = executeSchemaStatement(database, statement);
        if (!status)
        {
            return status;
        }
    }

    return {};
}

Status addLegacyColumns(
    SqliteDatabase& database
    )
{
    for (const SchemaColumn& column : SchemaColumns)
    {
        const Status status = ensureTableColumn(database, column);
        if (!status)
        {
            return status;
        }
    }

    return {};
}

Status preflightLegacyData(
    SqliteDatabase& database
    )
{
    const Status repairedUnassignedTeachers = database.execute(
        "UPDATE class_info SET teacher_id=NULL WHERE teacher_id <= 0"
        );
    if (!repairedUnassignedTeachers)
    {
        return repairedUnassignedTeachers;
    }

    const LegacyPreflightCheck checks[] = {
        {
            "class information records without a class",
            "SELECT COUNT(*) FROM class_info ci "
            "LEFT JOIN classes c ON c.id=ci.class_id "
            "WHERE c.id IS NULL"
        },
        {
            "class information records with an unknown teacher",
            "SELECT COUNT(*) FROM class_info ci "
            "LEFT JOIN teachers t ON t.id=ci.teacher_id "
            "WHERE ci.teacher_id IS NOT NULL AND t.id IS NULL"
        },
        {
            "testing class records without a class",
            "SELECT COUNT(*) FROM testing_classes tc "
            "LEFT JOIN classes c ON c.id=tc.class_id "
            "WHERE c.id IS NULL"
        },
        {
            "class-time records without a class",
            "SELECT COUNT(*) FROM class_times ct "
            "LEFT JOIN classes c ON c.id=ct.class_id "
            "WHERE c.id IS NULL"
        },
        {
            "intensive class-time records without a class",
            "SELECT COUNT(*) FROM class_intensive_times ct "
            "LEFT JOIN classes c ON c.id=ct.class_id "
            "WHERE c.id IS NULL"
        },
        {
            "roster column records without a class",
            "SELECT COUNT(*) FROM roster_columns rc "
            "LEFT JOIN classes c ON c.id=rc.class_id "
            "WHERE c.id IS NULL"
        },
        {
            "roster cell records without a class",
            "SELECT COUNT(*) FROM roster_data rd "
            "LEFT JOIN classes c ON c.id=rd.class_id "
            "WHERE c.id IS NULL"
        },
        {
            "roster columns with an invalid position",
            "SELECT COUNT(*) FROM roster_columns "
            "WHERE position IS NULL OR position < 0"
        },
        {
            "roster cells with an invalid row or column index",
            "SELECT COUNT(*) FROM roster_data "
            "WHERE row_index IS NULL OR row_index < 0 "
            "OR col_index IS NULL OR col_index < 0"
        },
        {
            "speaking evaluations without a class",
            "SELECT COUNT(*) FROM speaking_evaluations se "
            "LEFT JOIN classes c ON c.id=se.class_id "
            "WHERE c.id IS NULL"
        },
        {
            "speaking-evaluation rows without an evaluation",
            "SELECT COUNT(*) FROM speaking_eval_data sed "
            "LEFT JOIN speaking_evaluations se ON se.id=sed.evaluation_id "
            "WHERE se.id IS NULL"
        },
        {
            "speaking-evaluation rows with an invalid row index",
            "SELECT COUNT(*) FROM speaking_eval_data "
            "WHERE row_index IS NULL OR row_index < 0"
        },
        {
            "testing assignments with an unknown class",
            "SELECT COUNT(*) FROM schedule_testing_blocks stb "
            "LEFT JOIN classes c ON c.id=stb.class_id "
            "WHERE stb.class_id IS NOT NULL AND c.id IS NULL"
        },
        {
            "teachers with an unsupported internet type",
            "SELECT COUNT(*) FROM teachers "
            "WHERE internet_type IS NULL "
            "OR internet_type NOT IN ('WiFi', 'LAN', 'Both', 'N/A')"
        },
        {
            "teachers with an unsupported projection type",
            "SELECT COUNT(*) FROM teachers "
            "WHERE projection_type IS NULL "
            "OR projection_type NOT IN ('HDMI', 'Zoom', 'Any', 'N/A')"
        },
        {
            "calendar events with an unsupported event type",
            "SELECT COUNT(*) FROM calendar_events "
            "WHERE event_type IS NULL OR event_type NOT IN "
            "('Vacation', 'Holiday', 'Workshop', 'CM', 'Meeting', 'Other')"
        },
        {
            "calendar events with an unsupported time status",
            "SELECT COUNT(*) FROM calendar_events "
            "WHERE time_status IS NULL OR time_status NOT IN "
            "('Timed', 'Unknown', 'Unconfirmed')"
        },
        {
            "calendar events with an invalid all-day value",
            "SELECT COUNT(*) FROM calendar_events "
            "WHERE all_day IS NULL OR all_day NOT IN (0, 1)"
        }
    };

    for (const LegacyPreflightCheck& check : checks)
    {
        const auto result = database.query(check.countQuery);
        if (!result)
        {
            return std::unexpected(result.error());
        }

        const Result<std::int64_t> invalidRows = firstInteger(
            *result,
            "legacy database preflight count"
            );
        if (!invalidRows)
        {
            return std::unexpected(invalidRows.error());
        }
        if (*invalidRows > 0)
        {
            return std::unexpected(makeError(
                ErrorCode::Constraint,
                "Legacy database preflight found "
                + std::to_string(*invalidRows)
                + " invalid record(s): "
                + std::string(check.description)
                + ". Correct the affected data before opening this profile."
                ));
        }
    }

    return {};
}

Status rebuildConstrainedTable(
    SqliteDatabase& database,
    const ConstrainedSchemaTable& table
    )
{
    const std::string tableName = quoteIdentifier(table.tableName);
    const std::string legacyName =
        "classmngr_legacy_" + std::string(table.tableName);
    const std::string legacyTableName = quoteIdentifier(legacyName);

    const Status renamed = database.execute(
        "ALTER TABLE "
        + tableName
        + " RENAME TO "
        + legacyTableName
        );
    if (!renamed)
    {
        return renamed;
    }

    const Status created = executeSchemaStatement(
        database,
        SchemaStatement{table.sql, "table", table.tableName}
        );
    if (!created)
    {
        return created;
    }

    const std::string columns(table.columnNames);
    const Status copied = database.execute(
        "INSERT INTO "
        + tableName
        + " ("
        + columns
        + ") SELECT "
        + columns
        + " FROM "
        + legacyTableName
        );
    if (!copied)
    {
        return copied;
    }

    return database.execute("DROP TABLE " + legacyTableName);
}

Status rebuildConstrainedTables(
    SqliteDatabase& database
    )
{
    for (const ConstrainedSchemaTable& table : ConstrainedSchemaTables)
    {
        const Status rebuilt = rebuildConstrainedTable(database, table);
        if (!rebuilt)
        {
            return rebuilt;
        }
    }

    return {};
}

bool hasRowIndexConstraints(
    const ConstrainedSchemaTable& table
    ) noexcept
{
    return table.tableName == "roster_columns"
        || table.tableName == "roster_data"
        || table.tableName == "speaking_eval_data";
}

Status rebuildRowIndexConstraintTables(
    SqliteDatabase& database
    )
{
    for (const ConstrainedSchemaTable& table : ConstrainedSchemaTables)
    {
        if (!hasRowIndexConstraints(table))
        {
            continue;
        }

        const Status rebuilt = rebuildConstrainedTable(database, table);
        if (!rebuilt)
        {
            return rebuilt;
        }
    }

    return {};
}

Status createSchemaIndexes(
    SqliteDatabase& database
    )
{
    for (const SchemaStatement& statement : SchemaIndexStatements)
    {
        const Status status = executeSchemaStatement(database, statement);
        if (!status)
        {
            return status;
        }
    }

    return {};
}

std::string pathToUtf8(
    const std::filesystem::path& path
    )
{
    const std::u8string encoded = path.u8string();
    return std::string(
        reinterpret_cast<const char*>(encoded.data()),
        encoded.size()
        );
}

Status createPreConstraintMigrationBackup(
    SqliteDatabase& database,
    int migrationVersion
    )
{
    const std::string databasePath(database.databasePath());
    if (databasePath.empty() || databasePath == ":memory:")
    {
        return {};
    }

    std::filesystem::path path;
    try
    {
        path = std::filesystem::u8path(
            databasePath.begin(),
            databasePath.end()
            );
    }
    catch (const std::exception& exception)
    {
        return std::unexpected(makeError(
            ErrorCode::Io,
            "Unable to interpret the database path for migration backup: "
            + std::string(exception.what())
            ));
    }

    std::error_code filesystemError;
    const std::filesystem::path absolutePath = std::filesystem::absolute(
        path,
        filesystemError
        );
    if (filesystemError)
    {
        return std::unexpected(makeError(
            ErrorCode::Io,
            "Unable to resolve the database path for migration backup: "
            + filesystemError.message()
            ));
    }

    if (!std::filesystem::exists(absolutePath, filesystemError))
    {
        if (filesystemError)
        {
            return std::unexpected(makeError(
                ErrorCode::Io,
                "Unable to inspect the database before migration backup: "
                + filesystemError.message()
                ));
        }
        return {};
    }
    if (!std::filesystem::is_regular_file(absolutePath, filesystemError))
    {
        if (filesystemError)
        {
            return std::unexpected(makeError(
                ErrorCode::Io,
                "Unable to inspect the database before migration backup: "
                + filesystemError.message()
                ));
        }
        return {};
    }

    const std::string backupPath = pathToUtf8(absolutePath)
        + ".pre-schema-v"
        + std::to_string(migrationVersion)
        + "-backup";
    filesystemError.clear();
    if (std::filesystem::exists(
            std::filesystem::u8path(backupPath.begin(), backupPath.end()),
            filesystemError
            ))
    {
        return {};
    }
    if (filesystemError)
    {
        return std::unexpected(makeError(
            ErrorCode::Io,
            "Unable to inspect the migration backup path: "
            + filesystemError.message()
            ));
    }

    return database.execute(
        "VACUUM INTO " + quoteSqlLiteral(backupPath)
        );
}

Status runMigration(
    SqliteDatabase& database,
    const SchemaMigration& migration
    )
{
    auto transactionResult = database.beginTransaction();
    if (!transactionResult)
    {
        return std::unexpected(migrationError(
            migration,
            transactionResult.error()
            ));
    }
    SqliteTransaction transaction = std::move(*transactionResult);

    const Status migrated = migration.apply(database);
    if (!migrated)
    {
        return std::unexpected(migrationError(migration, migrated.error()));
    }

    const Status versionWritten = database.setSchemaVersion(migration.version);
    if (!versionWritten)
    {
        return std::unexpected(migrationError(
            migration,
            versionWritten.error()
            ));
    }

    const Status committed = transaction.commit();
    if (!committed)
    {
        return std::unexpected(migrationError(
            migration,
            committed.error()
            ));
    }

    return {};
}

const SchemaMigration SchemaMigrations[] = {
    { 1, "initial schema", createInitialSchema },
    { 2, "legacy column upgrades", addLegacyColumns },
    { 3, "legacy data preflight", preflightLegacyData },
    { 4, "foreign keys and stable constraints", rebuildConstrainedTables },
    { 5, "schema indexes", createSchemaIndexes },
    { 6, "valid persisted row indexes", rebuildRowIndexConstraintTables }
};
} // namespace

Status DatabaseSchemaManager::ensureSchema(
    SqliteDatabase& database
    )
{
    if (!database.isOpen())
    {
        return std::unexpected(makeError(
            ErrorCode::InvalidArgument,
            "Database schema setup requires an open database."
            ));
    }

    const Status foreignKeysEnabled =
        enableForeignKeyEnforcement(database);
    if (!foreignKeysEnabled)
    {
        return foreignKeysEnabled;
    }

    const Result<int> version = schemaVersion(database);
    if (!version)
    {
        return std::unexpected(version.error());
    }
    const auto existingProfileQuery = database.query(
        "SELECT EXISTS("
        "SELECT 1 FROM sqlite_master "
        "WHERE type='table' AND name NOT LIKE 'sqlite_%'"
        ")"
        );
    if (!existingProfileQuery)
    {
        return std::unexpected(existingProfileQuery.error());
    }
    const Result<std::int64_t> existingProfileValue = firstInteger(
        *existingProfileQuery,
        "whether the database has user tables"
        );
    if (!existingProfileValue)
    {
        return std::unexpected(existingProfileValue.error());
    }
    const bool existingProfile = *existingProfileValue != 0;

    if (*version > DatabaseSchemaManager::LatestSchemaVersion)
    {
        return std::unexpected(makeError(
            ErrorCode::Unsupported,
            "This Teacher Profile uses schema version "
            + std::to_string(*version)
            + ", but this version of ClassMngr supports schema version "
            + std::to_string(DatabaseSchemaManager::LatestSchemaVersion)
            + "."
            ));
    }

    int currentVersion = *version;
    for (const SchemaMigration& migration : SchemaMigrations)
    {
        if (migration.version <= currentVersion)
        {
            continue;
        }

        if (migration.version == 4 || migration.version == 6)
        {
            const bool requiresBackup = existingProfile
                && (migration.version == 4 || *version >= 5);
            if (requiresBackup)
            {
                const Status backupCreated =
                    createPreConstraintMigrationBackup(
                        database,
                        migration.version
                        );
                if (!backupCreated)
                {
                    return backupCreated;
                }
            }

            const Status foreignKeysDisabled = setForeignKeyEnforcement(
                database,
                false
                );
            if (!foreignKeysDisabled)
            {
                return foreignKeysDisabled;
            }

            const Status migrated = runMigration(database, migration);
            const Status reenabled = setForeignKeyEnforcement(database, true);
            if (!migrated)
            {
                return migrated;
            }
            if (!reenabled)
            {
                return reenabled;
            }
        }
        else
        {
            const Status migrated = runMigration(database, migration);
            if (!migrated)
            {
                return migrated;
            }
        }

        currentVersion = migration.version;
    }

    return verifyForeignKeyIntegrity(database);
}

Status DatabaseSchemaManager::enableForeignKeyEnforcement(
    SqliteDatabase& database
    )
{
    return setForeignKeyEnforcement(database, true);
}

Result<int> DatabaseSchemaManager::schemaVersion(
    const SqliteDatabase& database
    )
{
    return database.schemaVersion();
}

} // namespace classmngr::engine
