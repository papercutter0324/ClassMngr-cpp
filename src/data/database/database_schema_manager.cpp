#include "database_schema_manager.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"

#include <QObject>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace
{
struct SchemaStatement
{
    const char* sql;
    const char* objectType;
    const char* objectName;
};

struct SchemaColumn
{
    const char* tableName;
    const char* columnName;
    const char* definition;
};

struct SchemaMigration
{
    int version;
    const char* name;
    Status (*apply)(QSqlDatabase& database);
};

struct ConstrainedSchemaTable
{
    const char* tableName;
    const char* sql;
    const char* columnNames;
};

Status executeSchemaStatement(
    QSqlDatabase& database,
    const SchemaStatement& statement
    )
{
    const QString objectType = QString::fromLatin1(statement.objectType);
    const QString objectName = QString::fromLatin1(statement.objectName);
    const QString action = QObject::tr("Creating database %1")
        .arg(objectType);
    const QString identity = QObject::tr("%1 '%2'")
        .arg(objectType, objectName);

    QSqlQuery query(database);
    const auto executed = SqlQueryUtils::execute(
        query,
        QString::fromLatin1(statement.sql),
        action,
        identity
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    QSqlQuery verificationQuery(database);
    verificationQuery.prepare(QStringLiteral(
        "SELECT type FROM sqlite_master WHERE name=?"
        ));
    verificationQuery.addBindValue(objectName);
    const auto verified = SqlQueryUtils::executePrepared(
        verificationQuery,
        QObject::tr("Verifying database %1").arg(objectType),
        identity
        );
    if (!verified)
    {
        return std::unexpected(verified.error().userMessage());
    }
    if (!verificationQuery.next())
    {
        return std::unexpected(
            QObject::tr("Creating database %1 '%2' did not create the expected object.")
                .arg(objectType, objectName)
            );
    }

    const QString actualType = verificationQuery.value(0).toString();
    if (actualType != objectType)
    {
        return std::unexpected(
            QObject::tr(
                "Expected database %1 '%2', but an existing %3 has that name."
                ).arg(objectType, objectName, actualType)
            );
    }

    return {};
}

Result<bool> tableHasColumn(
    QSqlDatabase& database,
    const QString& tableName,
    const QString& columnName
    )
{
    const QString queryText = QStringLiteral("PRAGMA table_info(%1)")
        .arg(tableName);
    QSqlQuery query(database);
    const auto executed = SqlQueryUtils::execute(
        query,
        queryText,
        QObject::tr("Inspecting database columns"),
        QObject::tr("table '%1'").arg(tableName)
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        if (query.value(QStringLiteral("name")).toString() == columnName)
        {
            return true;
        }
    }

    return false;
}

Status ensureTableColumn(
    QSqlDatabase& database,
    const SchemaColumn& column
    )
{
    const QString tableName = QString::fromLatin1(column.tableName);
    const QString columnName = QString::fromLatin1(column.columnName);
    const Result<bool> hasColumn = tableHasColumn(
        database,
        tableName,
        columnName
        );
    if (!hasColumn)
    {
        return std::unexpected(hasColumn.error());
    }
    if (*hasColumn)
    {
        return {};
    }

    const QString queryText = QStringLiteral(
        "ALTER TABLE %1 ADD COLUMN %2 %3"
        ).arg(
            tableName,
            columnName,
            QString::fromLatin1(column.definition)
            );
    QSqlQuery query(database);
    const auto executed = SqlQueryUtils::execute(
        query,
        queryText,
        QObject::tr("Adding database column"),
        QObject::tr("column '%1.%2'").arg(tableName, columnName)
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    return {};
}

const SchemaStatement SchemaStatements[] = {
    { R"(
        CREATE TABLE IF NOT EXISTS app_settings (
            key TEXT PRIMARY KEY,
            value TEXT
        )
    )", "table", "app_settings" },
    { R"(
        CREATE TABLE IF NOT EXISTS roster_columns (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            class_id INTEGER,
            name TEXT,
            position INTEGER,
            width INTEGER
        )
    )", "table", "roster_columns" },
    { R"(
        CREATE TABLE IF NOT EXISTS roster_data (
            class_id INTEGER,
            row_index INTEGER,
            col_index INTEGER,
            value TEXT,
            PRIMARY KEY (class_id, row_index, col_index)
        )
    )", "table", "roster_data" },
    { R"(
        CREATE TABLE IF NOT EXISTS speaking_evaluations (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            class_id INTEGER,
            evaluation_name TEXT,
            UNIQUE(class_id, evaluation_name)
        )
    )", "table", "speaking_evaluations" },
    { R"(
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
    )", "table", "speaking_eval_data" },
    { R"(
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
    )", "table", "campuses" },
    { R"(
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
    )", "table", "teachers" },
    { R"(
        CREATE TABLE IF NOT EXISTS native_english_teachers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT,
            position TEXT,
            phone_number TEXT,
            birthday TEXT,
            nationality TEXT,
            email TEXT
        )
    )", "table", "native_english_teachers" },
    { R"(
        CREATE TABLE IF NOT EXISTS gs_team (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT,
            korean_name TEXT,
            position TEXT,
            phone_number TEXT,
            birthday TEXT
        )
    )", "table", "gs_team" },
    { R"(
        CREATE TABLE IF NOT EXISTS classes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT
        )
    )", "table", "classes" },
    { R"(
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
    )", "table", "class_info" },
    { R"(
        CREATE TABLE IF NOT EXISTS testing_classes (
            class_id INTEGER PRIMARY KEY,
            room TEXT NOT NULL
        )
    )", "table", "testing_classes" },
    { R"(
        CREATE TABLE IF NOT EXISTS class_times (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            class_id INTEGER,
            day TEXT,
            start_time TEXT,
            end_time TEXT
        )
    )", "table", "class_times" },
    { R"(
        CREATE TABLE IF NOT EXISTS class_intensive_times (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            class_id INTEGER,
            day TEXT,
            start_time TEXT,
            end_time TEXT,
            UNIQUE(day, start_time)
        )
    )", "table", "class_intensive_times" },
    { R"(
        CREATE TABLE IF NOT EXISTS intensive_slot_states (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            day TEXT,
            start_time TEXT,
            state TEXT,
            UNIQUE(day, start_time)
        )
    )", "table", "intensive_slot_states" },
    { R"(
        CREATE TABLE IF NOT EXISTS schedule_testing_blocks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            day TEXT NOT NULL,
            start_time TEXT NOT NULL,
            room TEXT NOT NULL DEFAULT '',
            class_id INTEGER,
            UNIQUE(day, start_time)
        )
    )", "table", "schedule_testing_blocks" },
    { R"(
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
    )", "table", "calendar_events" }
};

const SchemaStatement SchemaIndexStatements[] = {
    { R"(
        CREATE INDEX IF NOT EXISTS idx_schedule_testing_blocks_class_id
        ON schedule_testing_blocks (class_id)
    )", "index", "idx_schedule_testing_blocks_class_id" },
    { R"(
        CREATE INDEX IF NOT EXISTS idx_calendar_events_dates
        ON calendar_events (start_date, end_date, start_time, title)
    )", "index", "idx_calendar_events_dates" },
    { R"(
        CREATE INDEX IF NOT EXISTS idx_calendar_events_end_dates
        ON calendar_events (end_date, start_date, start_time, title)
    )", "index", "idx_calendar_events_end_dates" },
    { R"(
        CREATE INDEX IF NOT EXISTS idx_calendar_events_repeat_series
        ON calendar_events (repeat_series_id, start_date, id)
    )", "index", "idx_calendar_events_repeat_series" }
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
        R"(
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
        )",
        "id, teacher_kr, teacher_en, preferred_romanization, preferred_name, "
        "room_number, birthday, phone_number, wifi_name, wifi_password, "
        "internet_type, zoom_id, zoom_password, projection_type, notes"
    },
    {
        "classes",
        R"(
            CREATE TABLE IF NOT EXISTS classes (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT
            )
        )",
        "id, name"
    },
    {
        "class_info",
        R"(
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
        )",
        "class_id, teacher_id, class_grade, class_level, reading_book, "
        "essay_book, class_color, font_color, notes, time_filler_activities"
    },
    {
        "testing_classes",
        R"(
            CREATE TABLE IF NOT EXISTS testing_classes (
                class_id INTEGER PRIMARY KEY
                    REFERENCES classes(id) ON DELETE CASCADE,
                room TEXT NOT NULL
            )
        )",
        "class_id, room"
    },
    {
        "class_times",
        R"(
            CREATE TABLE IF NOT EXISTS class_times (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                class_id INTEGER NOT NULL
                    REFERENCES classes(id) ON DELETE CASCADE,
                day TEXT,
                start_time TEXT,
                end_time TEXT
            )
        )",
        "id, class_id, day, start_time, end_time"
    },
    {
        "class_intensive_times",
        R"(
            CREATE TABLE IF NOT EXISTS class_intensive_times (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                class_id INTEGER NOT NULL
                    REFERENCES classes(id) ON DELETE CASCADE,
                day TEXT,
                start_time TEXT,
                end_time TEXT,
                UNIQUE(day, start_time)
            )
        )",
        "id, class_id, day, start_time, end_time"
    },
    {
        "roster_columns",
        R"(
            CREATE TABLE IF NOT EXISTS roster_columns (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                class_id INTEGER NOT NULL
                    REFERENCES classes(id) ON DELETE CASCADE,
                name TEXT,
                position INTEGER NOT NULL CHECK(position >= 0),
                width INTEGER
            )
        )",
        "id, class_id, name, position, width"
    },
    {
        "roster_data",
        R"(
            CREATE TABLE IF NOT EXISTS roster_data (
                class_id INTEGER NOT NULL
                    REFERENCES classes(id) ON DELETE CASCADE,
                row_index INTEGER NOT NULL CHECK(row_index >= 0),
                col_index INTEGER NOT NULL CHECK(col_index >= 0),
                value TEXT,
                PRIMARY KEY (class_id, row_index, col_index)
            )
        )",
        "class_id, row_index, col_index, value"
    },
    {
        "speaking_evaluations",
        R"(
            CREATE TABLE IF NOT EXISTS speaking_evaluations (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                class_id INTEGER NOT NULL
                    REFERENCES classes(id) ON DELETE CASCADE,
                evaluation_name TEXT,
                UNIQUE(class_id, evaluation_name)
            )
        )",
        "id, class_id, evaluation_name"
    },
    {
        "speaking_eval_data",
        R"(
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
        )",
        "evaluation_id, row_index, col_0, col_1, col_2, col_3, col_4, "
        "col_5, col_6, col_7, col_8, col_9, col_10"
    },
    {
        "schedule_testing_blocks",
        R"(
            CREATE TABLE IF NOT EXISTS schedule_testing_blocks (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                day TEXT NOT NULL,
                start_time TEXT NOT NULL,
                room TEXT NOT NULL DEFAULT '',
                class_id INTEGER
                    REFERENCES classes(id) ON DELETE SET NULL,
                UNIQUE(day, start_time)
            )
        )",
        "id, day, start_time, room, class_id"
    },
    {
        "calendar_events",
        R"(
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
        )",
        "id, title, event_type, time_status, repeat_series_id, all_day, "
        "start_date, start_time, end_date, end_time"
    }
};

Status setForeignKeyEnforcement(
    QSqlDatabase& database,
    bool enabled
    )
{
    const QString setting = enabled
        ? QStringLiteral("ON")
        : QStringLiteral("OFF");
    QSqlQuery query(database);
    const auto configured = SqlQueryUtils::execute(
        query,
        QStringLiteral("PRAGMA foreign_keys = %1").arg(setting),
        QObject::tr("Configuring database foreign-key enforcement")
        );
    if (!configured)
    {
        return std::unexpected(configured.error().userMessage());
    }

    QSqlQuery verificationQuery(database);
    const auto verified = SqlQueryUtils::execute(
        verificationQuery,
        QStringLiteral("PRAGMA foreign_keys"),
        QObject::tr("Verifying database foreign-key enforcement")
        );
    if (!verified)
    {
        return std::unexpected(verified.error().userMessage());
    }
    if (!verificationQuery.next()
        || verificationQuery.value(0).toInt() != (enabled ? 1 : 0))
    {
        return std::unexpected(
            QObject::tr(
                "SQLite did not %1 foreign-key enforcement for this connection."
                ).arg(enabled
                    ? QObject::tr("enable")
                    : QObject::tr("disable"))
            );
    }

    return {};
}

Status verifyForeignKeyIntegrity(QSqlDatabase& database)
{
    QSqlQuery query(database);
    const auto executed = SqlQueryUtils::execute(
        query,
        QStringLiteral("PRAGMA foreign_key_check"),
        QObject::tr("Checking database foreign-key integrity")
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }
    if (!query.next())
    {
        return {};
    }

    return std::unexpected(
        QObject::tr(
            "Foreign-key integrity check failed for table '%1', row %2, "
            "referencing parent table '%3'."
            ).arg(
                query.value(0).toString(),
                query.value(1).toString(),
                query.value(2).toString()
                )
        );
}

Result<int> readSchemaVersion(QSqlDatabase& database)
{
    QSqlQuery query(database);
    const auto executed = SqlQueryUtils::execute(
        query,
        QStringLiteral("PRAGMA user_version"),
        QObject::tr("Reading database schema version")
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }
    if (!query.next())
    {
        return std::unexpected(
            QObject::tr("SQLite did not return a database schema version.")
            );
    }

    bool isNumber = false;
    const int version = query.value(0).toInt(&isNumber);
    if (!isNumber || version < 0)
    {
        return std::unexpected(
            QObject::tr("SQLite returned an invalid database schema version.")
            );
    }

    return version;
}

Result<bool> databaseHasUserTables(QSqlDatabase& database)
{
    QSqlQuery query(database);
    const auto executed = SqlQueryUtils::execute(
        query,
        QStringLiteral(
            "SELECT EXISTS("
            "SELECT 1 FROM sqlite_master "
            "WHERE type='table' AND name NOT LIKE 'sqlite_%'"
            ")"
            ),
        QObject::tr("Inspecting existing database schema")
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }
    if (!query.next())
    {
        return std::unexpected(
            QObject::tr("SQLite did not report whether the database has tables.")
            );
    }

    return query.value(0).toBool();
}

Status writeSchemaVersion(QSqlDatabase& database, int version)
{
    QSqlQuery query(database);
    const auto executed = SqlQueryUtils::execute(
        query,
        QStringLiteral("PRAGMA user_version = %1").arg(version),
        QObject::tr("Recording database schema version")
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    return {};
}

Status executeMigrationStatement(
    QSqlDatabase& database,
    const QString& sql,
    const QString& action,
    const QString& identity
    )
{
    QSqlQuery query(database);
    const auto executed = SqlQueryUtils::execute(
        query,
        sql,
        action,
        identity
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    return {};
}

Status createInitialSchema(QSqlDatabase& database)
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

Status addLegacyColumns(QSqlDatabase& database)
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

struct LegacyPreflightCheck
{
    const char* description;
    const char* countQuery;
};

Status preflightLegacyData(QSqlDatabase& database)
{
    const Status repairedUnassignedTeachers = executeMigrationStatement(
        database,
        QStringLiteral(
            "UPDATE class_info SET teacher_id=NULL WHERE teacher_id <= 0"
            ),
        QObject::tr("Repairing unassigned teachers in legacy class information"),
        QObject::tr("table 'class_info'")
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
        QSqlQuery query(database);
        const auto executed = SqlQueryUtils::execute(
            query,
            QString::fromLatin1(check.countQuery),
            QObject::tr("Checking legacy database data"),
            QString::fromLatin1(check.description)
            );
        if (!executed)
        {
            return std::unexpected(executed.error().userMessage());
        }
        if (!query.next())
        {
            return std::unexpected(
                QObject::tr("Legacy database preflight did not return a row count.")
                );
        }

        const int invalidRows = query.value(0).toInt();
        if (invalidRows > 0)
        {
            return std::unexpected(
                QObject::tr(
                    "Legacy database preflight found %1 invalid record(s): %2. "
                    "Correct the affected data before opening this profile."
                    ).arg(invalidRows).arg(QString::fromLatin1(check.description))
                );
        }
    }

    return {};
}

Status rebuildConstrainedTable(
    QSqlDatabase& database,
    const ConstrainedSchemaTable& table
    )
{
    const QString tableName = QString::fromLatin1(table.tableName);
    const QString legacyTableName =
        QStringLiteral("classmngr_legacy_") + tableName;
    const QString identity = QObject::tr("table '%1'").arg(tableName);

    const Status renamed = executeMigrationStatement(
        database,
        QStringLiteral("ALTER TABLE %1 RENAME TO %2").arg(
            tableName,
            legacyTableName
            ),
        QObject::tr("Preparing database table for constraint migration"),
        identity
        );
    if (!renamed)
    {
        return renamed;
    }

    const SchemaStatement statement{
        table.sql,
        "table",
        table.tableName
    };
    const Status created = executeSchemaStatement(database, statement);
    if (!created)
    {
        return created;
    }

    const QString columns = QString::fromLatin1(table.columnNames);
    const Status copied = executeMigrationStatement(
        database,
        QStringLiteral("INSERT INTO %1 (%2) SELECT %2 FROM %3").arg(
            tableName,
            columns,
            legacyTableName
            ),
        QObject::tr("Copying legacy database records into constrained table"),
        identity
        );
    if (!copied)
    {
        return copied;
    }

    return executeMigrationStatement(
        database,
        QStringLiteral("DROP TABLE %1").arg(legacyTableName),
        QObject::tr("Removing legacy database table after migration"),
        identity
        );
}

Status rebuildConstrainedTables(QSqlDatabase& database)
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

bool hasRowIndexConstraints(const ConstrainedSchemaTable& table)
{
    const QString tableName = QString::fromLatin1(table.tableName);
    return tableName == QStringLiteral("roster_columns")
        || tableName == QStringLiteral("roster_data")
        || tableName == QStringLiteral("speaking_eval_data");
}

Status rebuildRowIndexConstraintTables(QSqlDatabase& database)
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

Status createSchemaIndexes(QSqlDatabase& database)
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

Status createPreConstraintMigrationBackup(
    QSqlDatabase& database,
    int migrationVersion
    )
{
    const QString databasePath = database.databaseName();
    if (databasePath.isEmpty() || databasePath == QStringLiteral(":memory:"))
    {
        return {};
    }

    const QFileInfo databaseInfo(databasePath);
    if (!databaseInfo.exists() || !databaseInfo.isFile())
    {
        return {};
    }

    const QString backupPath = databaseInfo.absoluteFilePath()
        + QStringLiteral(".pre-schema-v%1-backup").arg(migrationVersion);
    if (QFile::exists(backupPath))
    {
        return {};
    }

    const QString escapedBackupPath = QString(backupPath).replace(
        QStringLiteral("'"),
        QStringLiteral("''")
        );
    return executeMigrationStatement(
        database,
        QStringLiteral("VACUUM INTO '%1'").arg(escapedBackupPath),
        QObject::tr("Backing up database before constraint migration"),
        QObject::tr("database '%1'").arg(databaseInfo.fileName())
        );
}

Status runMigration(
    QSqlDatabase& database,
    const SchemaMigration& migration
    )
{
    DatabaseTransaction transaction(database);
    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr("Starting database migration %1 (%2) failed: %3")
                .arg(migration.version)
                .arg(QString::fromLatin1(migration.name))
                .arg(database.lastError().text())
            );
    }

    const Status migrated = migration.apply(database);
    if (!migrated)
    {
        return std::unexpected(
            QObject::tr("Database migration %1 (%2) failed: %3")
                .arg(migration.version)
                .arg(QString::fromLatin1(migration.name))
                .arg(migrated.error())
            );
    }

    const Status versionWritten = writeSchemaVersion(database, migration.version);
    if (!versionWritten)
    {
        return std::unexpected(
            QObject::tr("Database migration %1 (%2) failed: %3")
                .arg(migration.version)
                .arg(QString::fromLatin1(migration.name))
                .arg(versionWritten.error())
            );
    }

    if (!transaction.commit())
    {
        return std::unexpected(
            QObject::tr("Committing database migration %1 (%2) failed: %3")
                .arg(migration.version)
                .arg(QString::fromLatin1(migration.name))
                .arg(database.lastError().text())
            );
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

Status DatabaseSchemaManager::ensureSchema(QSqlDatabase& database)
{
    if (!database.isValid() || !database.isOpen())
    {
        return std::unexpected(
            QObject::tr("Database schema setup requires an open database.")
            );
    }

    const Status foreignKeysEnabled = enableForeignKeyEnforcement(database);
    if (!foreignKeysEnabled)
    {
        return foreignKeysEnabled;
    }

    const Result<int> version = schemaVersion(database);
    if (!version)
    {
        return std::unexpected(version.error());
    }
    const Result<bool> existingProfile = databaseHasUserTables(database);
    if (!existingProfile)
    {
        return std::unexpected(existingProfile.error());
    }
    if (*version > LatestSchemaVersion)
    {
        return std::unexpected(
            QObject::tr(
                "This Teacher Profile uses schema version %1, but this version "
                "of ClassMngr supports schema version %2."
                ).arg(*version).arg(LatestSchemaVersion)
            );
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
            const bool requiresBackup = *existingProfile
                && (migration.version == 4 || *version >= 5);
            if (requiresBackup)
            {
                const Status backupCreated = createPreConstraintMigrationBackup(
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

Status DatabaseSchemaManager::enableForeignKeyEnforcement(QSqlDatabase& database)
{
    if (!database.isValid() || !database.isOpen())
    {
        return std::unexpected(
            QObject::tr("Enabling database foreign-key enforcement requires an open database.")
            );
    }

    return setForeignKeyEnforcement(database, true);
}

Result<int> DatabaseSchemaManager::schemaVersion(QSqlDatabase& database)
{
    if (!database.isValid() || !database.isOpen())
    {
        return std::unexpected(
            QObject::tr("Reading the database schema version requires an open database.")
            );
    }

    return readSchemaVersion(database);
}
