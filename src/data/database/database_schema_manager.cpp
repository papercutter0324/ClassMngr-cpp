#include "database_schema_manager.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

namespace
{
bool tableHasColumn(
    QSqlDatabase& database,
    const QString& tableName,
    const QString& columnName
    )
{
    QSqlQuery query(database);

    if (!query.exec(
            QString("PRAGMA table_info(%1)")
                .arg(tableName)
            ))
    {
        qWarning()
            << "Failed to inspect table columns for"
            << tableName
            << ":"
            << query.lastError().text();
        return false;
    }

    while (query.next())
    {
        if (query.value("name").toString() == columnName)
        {
            return true;
        }
    }

    return false;
}

void ensureTableColumn(
    QSqlDatabase& database,
    const QString& tableName,
    const QString& columnName,
    const QString& definition
    )
{
    if (tableHasColumn(database, tableName, columnName))
    {
        return;
    }

    QSqlQuery query(database);

    if (!query.exec(
            QString("ALTER TABLE %1 ADD COLUMN %2 %3")
                .arg(tableName, columnName, definition)
            ))
    {
        qWarning()
            << "Failed to add column"
            << columnName
            << "to"
            << tableName
            << ":"
            << query.lastError().text();
    }
}
} // namespace

void DatabaseSchemaManager::ensureSchema(QSqlDatabase& database)
{
    QSqlQuery query(database);

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS app_settings (
            key TEXT PRIMARY KEY,
            value TEXT
        )
    )");

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS roster_columns (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            class_id INTEGER,

            name TEXT,

            position INTEGER,

            width INTEGER
        )
    )");

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS roster_data (
            class_id INTEGER,

            row_index INTEGER,

            col_index INTEGER,

            value TEXT,

            PRIMARY KEY (
                class_id,
                row_index,
                col_index
            )
        )
    )");

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS speaking_evaluations (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            class_id INTEGER,

            evaluation_name TEXT,

            UNIQUE(
                class_id,
                evaluation_name
            )
        )
    )");

    query.exec(R"(
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

            PRIMARY KEY (
                evaluation_id,
                row_index
            )
        )
    )");

    query.exec(R"(
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
    )");

    ensureTableColumn(
        database,
        QStringLiteral("campuses"),
        QStringLiteral("building_name"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        database,
        QStringLiteral("campuses"),
        QStringLiteral("address"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        database,
        QStringLiteral("campuses"),
        QStringLiteral("phone_number"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        database,
        QStringLiteral("campuses"),
        QStringLiteral("office_number"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        database,
        QStringLiteral("campuses"),
        QStringLiteral("transit_steps"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        database,
        QStringLiteral("campuses"),
        QStringLiteral("arrival_info"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        database,
        QStringLiteral("campuses"),
        QStringLiteral("image_path"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        database,
        QStringLiteral("campuses"),
        QStringLiteral("office_wifi"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        database,
        QStringLiteral("campuses"),
        QStringLiteral("office_wifi_password"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        database,
        QStringLiteral("campuses"),
        QStringLiteral("printer_name"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        database,
        QStringLiteral("campuses"),
        QStringLiteral("printer_steps"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        database,
        QStringLiteral("campuses"),
        QStringLiteral("photocopier_code"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        database,
        QStringLiteral("campuses"),
        QStringLiteral("housing_locations"),
        QStringLiteral("TEXT")
        );

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS teachers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            teacher_kr TEXT,
            teacher_en TEXT,
            preferred_romanization TEXT,

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
    )");

    ensureTableColumn(
        database,
        QStringLiteral("teachers"),
        QStringLiteral("preferred_romanization"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        database,
        QStringLiteral("teachers"),
        QStringLiteral("birthday"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        database,
        QStringLiteral("teachers"),
        QStringLiteral("phone_number"),
        QStringLiteral("TEXT")
        );

    ensureTableColumn(
        database,
        QStringLiteral("teachers"),
        QStringLiteral("internet_type"),
        QStringLiteral("TEXT DEFAULT 'WiFi'")
        );

    ensureTableColumn(
        database,
        QStringLiteral("teachers"),
        QStringLiteral("projection_type"),
        QStringLiteral("TEXT DEFAULT 'HDMI'")
        );

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS classes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            name TEXT
        )
    )");

    query.exec(R"(
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
    )");

    ensureTableColumn(
        database,
        QStringLiteral("class_info"),
        QStringLiteral("time_filler_activities"),
        QStringLiteral("TEXT")
        );

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS class_times (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            class_id INTEGER,

            day TEXT,
            start_time TEXT,
            end_time TEXT
        )
    )");

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS class_intensive_times (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            class_id INTEGER,

            day TEXT,
            start_time TEXT,
            end_time TEXT,

            UNIQUE(day, start_time)
        )
    )");

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS intensive_slot_states (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            day TEXT,
            start_time TEXT,
            state TEXT,

            UNIQUE(day, start_time)
        )
    )");

    query.exec(R"(
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
    )");

    query.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_calendar_events_dates
        ON calendar_events (start_date, end_date, start_time, title)
    )");

    ensureTableColumn(
        database,
        QStringLiteral("calendar_events"),
        QStringLiteral("event_type"),
        QStringLiteral("TEXT DEFAULT 'Other'")
        );

    ensureTableColumn(
        database,
        QStringLiteral("calendar_events"),
        QStringLiteral("time_status"),
        QStringLiteral("TEXT DEFAULT 'Timed'")
        );

    ensureTableColumn(
        database,
        QStringLiteral("calendar_events"),
        QStringLiteral("all_day"),
        QStringLiteral("INTEGER DEFAULT 0")
        );

    ensureTableColumn(
        database,
        QStringLiteral("calendar_events"),
        QStringLiteral("repeat_series_id"),
        QStringLiteral("TEXT")
        );

    query.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_calendar_events_repeat_series
        ON calendar_events (repeat_series_id, start_date, id)
    )");
}
