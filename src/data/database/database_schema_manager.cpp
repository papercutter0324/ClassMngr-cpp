#include "database_schema_manager.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"

#include <QObject>
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
} // namespace

Status DatabaseSchemaManager::ensureSchema(QSqlDatabase& database)
{
    if (!database.isValid() || !database.isOpen())
    {
        return std::unexpected(
            QObject::tr("Database schema setup requires an open database.")
            );
    }

    DatabaseTransaction transaction(database);
    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr("Starting database schema transaction failed: %1")
                .arg(database.lastError().text())
            );
    }

    for (const SchemaStatement& statement : SchemaStatements)
    {
        const Status status = executeSchemaStatement(database, statement);
        if (!status)
        {
            return status;
        }
    }

    for (const SchemaColumn& column : SchemaColumns)
    {
        const Status status = ensureTableColumn(database, column);
        if (!status)
        {
            return status;
        }
    }

    for (const SchemaStatement& statement : SchemaIndexStatements)
    {
        const Status status = executeSchemaStatement(database, statement);
        if (!status)
        {
            return status;
        }
    }

    if (!transaction.commit())
    {
        return std::unexpected(
            QObject::tr("Committing database schema transaction failed: %1")
                .arg(database.lastError().text())
            );
    }

    return {};
}
