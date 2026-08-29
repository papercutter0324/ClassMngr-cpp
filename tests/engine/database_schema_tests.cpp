#include "classmngr/engine/database_schema.h"
#include "classmngr/engine/open_database.h"
#include "classmngr/engine/sqlite_database.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

namespace
{
using classmngr::engine::DatabaseSchemaManager;
using classmngr::engine::ErrorCode;
using classmngr::engine::OpenDatabase;
using classmngr::engine::SqliteDatabase;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineDatabaseSchemaTests: "
              << message
              << '\n';
    return false;
}

bool hasTable(
    SqliteDatabase& database,
    std::string_view tableName
    )
{
    const auto result = database.query(
        "SELECT EXISTS("
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?"
        ")",
        classmngr::engine::SqliteParameters{
            classmngr::engine::SqliteValue{std::string(tableName)}
        }
        );
    if (!result || result->rows.size() != 1
        || result->rows.front().values.size() != 1)
    {
        return false;
    }

    const auto* value = std::get_if<std::int64_t>(
        &result->rows.front().values.front()
        );
    return value != nullptr && *value != 0;
}

bool hasCount(
    SqliteDatabase& database,
    std::string_view sql,
    std::int64_t expected
    )
{
    const auto result = database.query(sql);
    if (!result || result->rows.size() != 1
        || result->rows.front().values.size() != 1)
    {
        return false;
    }

    const auto* value = std::get_if<std::int64_t>(
        &result->rows.front().values.front()
        );
    return value != nullptr && *value == expected;
}

bool hasNullValue(
    SqliteDatabase& database,
    std::string_view sql
    )
{
    const auto result = database.query(sql);
    if (!result || result->rows.size() != 1
        || result->rows.front().values.size() != 1)
    {
        return false;
    }

    return std::holds_alternative<std::monostate>(
        result->rows.front().values.front()
        );
}

bool openLegacyTables(
    SqliteDatabase& database,
    std::string_view sql
    )
{
    return database.open(":memory:").has_value()
        && database.execute(sql).has_value();
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

bool ensureLatest(
    SqliteDatabase& database
    )
{
    if (!database.isOpen() && !database.open(":memory:"))
    {
        return false;
    }

    const auto status = DatabaseSchemaManager::ensureSchema(database);
    const auto version = DatabaseSchemaManager::schemaVersion(database);
    return status && version
        && *version == DatabaseSchemaManager::LatestSchemaVersion;
}
} // namespace

int main()
{
    bool passed = true;

    {
        SqliteDatabase database;
        passed &= expect(
            ensureLatest(database),
            "fresh database did not migrate to the latest schema"
            );
        passed &= expect(
            hasTable(database, "classes")
                && hasTable(database, "calendar_events")
                && hasTable(database, "schedule_testing_blocks"),
            "fresh schema did not create representative tables"
            );

        const auto foreignKeys = database.foreignKeysEnabled();
        passed &= expect(
            foreignKeys && *foreignKeys,
            "latest schema did not leave foreign keys enabled"
            );
        passed &= expect(
            !database.execute(
                "INSERT INTO class_times "
                "(class_id, day, start_time, end_time) "
                "VALUES (999, 'Monday', '09:00', '10:00')"
                ),
            "latest schema accepted an orphaned class-time row"
            );
        passed &= expect(
            database.execute(
                "INSERT INTO classes (id, name) VALUES (1, 'Schema Test')"
                ).has_value(),
            "latest schema did not accept a class row"
            );
        passed &= expect(
            database.execute(
                "INSERT INTO roster_data "
                "(class_id, row_index, col_index, value) "
                "VALUES (1, 0, 0, 'Student')"
                ).has_value(),
            "latest schema did not accept a roster row"
            );
        passed &= expect(
            database.execute("DELETE FROM classes WHERE id=1").has_value()
                && hasCount(database, "SELECT COUNT(*) FROM roster_data", 0),
            "cascade behavior was not preserved"
            );
    }

    {
        SqliteDatabase database;
        passed &= expect(
            openLegacyTables(
                database,
                R"SQL(
                    CREATE TABLE classes (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        name TEXT
                    );
                    CREATE TABLE class_info (
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
                    );
                    INSERT INTO classes (id, name)
                        VALUES (1, 'Legacy Class');
                    INSERT INTO class_info (class_id, teacher_id)
                        VALUES (1, -1);
                )SQL"
                ),
            "legacy schema fixture could not be created"
            );
        passed &= expect(
            ensureLatest(database),
            "legacy schema did not migrate to the latest version"
            );
        passed &= expect(
            hasNullValue(
                database,
                "SELECT teacher_id FROM class_info WHERE class_id=1"
                ),
            "legacy unassigned teacher was not repaired"
            );
        passed &= expect(
            !database.execute(
                "INSERT INTO class_times "
                "(class_id, day, start_time, end_time) "
                "VALUES (999, 'Monday', '09:00', '10:00')"
                ),
            "migrated legacy schema accepted an orphaned row"
            );
    }

    {
        SqliteDatabase database;
        passed &= expect(
            openLegacyTables(
                database,
                R"SQL(
                    CREATE TABLE teachers (
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
                    );
                    INSERT INTO teachers (teacher_en, internet_type)
                        VALUES ('Legacy Teacher', 'Satellite');
                )SQL"
                ),
            "invalid legacy fixture could not be created"
            );
        const auto status = DatabaseSchemaManager::ensureSchema(database);
        const auto version = database.schemaVersion();
        passed &= expect(
            !status && status.error().code == ErrorCode::Migration
                && status.error().message.find("unsupported internet type")
                    != std::string::npos,
            "invalid legacy enum was not rejected during preflight"
            );
        passed &= expect(
            version && *version == 2,
            "invalid legacy enum advanced past the preflight migration"
            );
    }

    {
        SqliteDatabase database;
        passed &= expect(
            openLegacyTables(
                database,
                R"SQL(
                    CREATE TABLE classes (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        name TEXT
                    );
                    CREATE TABLE roster_data (
                        class_id INTEGER,
                        row_index INTEGER,
                        col_index INTEGER,
                        value TEXT,
                        PRIMARY KEY (class_id, row_index, col_index)
                    );
                    INSERT INTO classes (id, name)
                        VALUES (1, 'Legacy Class');
                    INSERT INTO roster_data
                        (class_id, row_index, col_index, value)
                        VALUES (1, -1, 0, 'Invalid');
                )SQL"
                ),
            "invalid row-index fixture could not be created"
            );
        const auto status = DatabaseSchemaManager::ensureSchema(database);
        passed &= expect(
            !status && status.error().code == ErrorCode::Migration
                && status.error().message.find("invalid row or column index")
                    != std::string::npos,
            "invalid legacy row index was not rejected"
            );
        const auto version = database.schemaVersion();
        passed &= expect(
            version && *version == 2,
            "invalid row index advanced past the preflight migration"
            );
    }

    {
        std::error_code filesystemError;
        const auto uniqueSuffix = std::chrono::steady_clock::now()
            .time_since_epoch()
            .count();
        const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("classmngr-engine-schema-" + std::to_string(uniqueSuffix));
        std::filesystem::create_directory(directory, filesystemError);
        passed &= expect(
            !filesystemError,
            "temporary backup directory could not be created"
            );
        if (!filesystemError)
        {
            const std::filesystem::path databasePath =
                directory / "legacy-profile.db";
            SqliteDatabase database;
            passed &= expect(
                database.open(pathToUtf8(databasePath)).has_value()
                    && database.execute(
                        "CREATE TABLE classes ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT)"
                        ).has_value()
                    && ensureLatest(database),
                "file-backed legacy schema did not migrate"
                );
            const std::filesystem::path backupPath =
                std::filesystem::u8path(
                    pathToUtf8(databasePath)
                    + ".pre-schema-v4-backup"
                    );
            passed &= expect(
                std::filesystem::exists(backupPath),
                "file-backed constraint migration did not create a backup"
                );
            database.close();
            std::filesystem::remove_all(directory, filesystemError);
            passed &= expect(
                !filesystemError,
                "temporary backup directory could not be removed"
                );
        }
    }

    {
        SqliteDatabase database;
        passed &= expect(
            openLegacyTables(
                database,
                R"SQL(
                    CREATE TABLE teachers (
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
                    );
                    CREATE TABLE classes (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        name TEXT
                    );
                    CREATE TABLE classmngr_legacy_classes (
                        id INTEGER PRIMARY KEY
                    );
                    INSERT INTO teachers (teacher_en)
                        VALUES ('Legacy Teacher');
                )SQL"
                ),
            "rollback fixture could not be created"
            );
        const auto status = DatabaseSchemaManager::ensureSchema(database);
        const auto version = database.schemaVersion();
        passed &= expect(
            !status && status.error().code == ErrorCode::Migration
                && status.error().message.find("migration 4")
                    != std::string::npos,
            "partial constraint migration did not fail with migration context"
            );
        passed &= expect(
            version && *version == 3
                && hasTable(database, "teachers")
                && hasTable(database, "classmngr_legacy_classes")
                && !hasTable(database, "classmngr_legacy_teachers"),
            "failed constraint migration did not roll back atomically"
            );
    }

    {
        SqliteDatabase database;
        passed &= expect(
            ensureLatest(database),
            "version-five fixture could not create its base schema"
            );
        passed &= expect(
            database.execute(
                "INSERT INTO classes (id, name) VALUES (1, 'Version Five')"
                ).has_value()
                && database.execute(
                    "INSERT INTO roster_columns "
                    "(class_id, name, position, width) "
                    "VALUES (1, 'Name', 0, 180)"
                    ).has_value()
                && database.execute(
                    "INSERT INTO roster_data "
                    "(class_id, row_index, col_index, value) "
                    "VALUES (1, 0, 0, 'Student')"
                    ).has_value(),
            "version-five fixture data could not be inserted"
            );
        passed &= expect(
            database.setSchemaVersion(5).has_value()
                && ensureLatest(database),
            "version-five schema did not receive row-index constraints"
            );
        passed &= expect(
            !database.execute(
                "INSERT INTO roster_data "
                "(class_id, row_index, col_index, value) "
                "VALUES (1, -1, 0, 'Invalid')"
                ),
            "row-index migration accepted a negative row index"
            );
    }

    {
        SqliteDatabase database;
        passed &= expect(
            ensureLatest(database),
            "foreign-key integrity fixture could not create its base schema"
            );
        passed &= expect(
            database.execute("PRAGMA foreign_keys = OFF").has_value()
                && database.execute(
                    "INSERT INTO class_times "
                    "(class_id, day, start_time, end_time) "
                    "VALUES (999, 'Monday', '09:00', '10:00')"
                    ).has_value(),
            "foreign-key integrity fixture could not insert its orphan"
            );
        const auto status = DatabaseSchemaManager::ensureSchema(database);
        passed &= expect(
            !status && status.error().code == ErrorCode::Constraint
                && status.error().message.find("Foreign-key integrity")
                    != std::string::npos,
            "foreign-key integrity failure was not reported"
            );
    }

    {
        SqliteDatabase database;
        passed &= expect(
            database.open(":memory:").has_value()
                && database.setSchemaVersion(
                    DatabaseSchemaManager::LatestSchemaVersion + 1
                    ).has_value(),
            "newer-schema fixture could not be created"
            );
        const auto status = DatabaseSchemaManager::ensureSchema(database);
        passed &= expect(
            !status && status.error().code == ErrorCode::Unsupported
                && !hasTable(database, "classes"),
            "newer schema version was not rejected"
            );
    }

    {
        const auto opened = OpenDatabase::execute(":memory:");
        passed &= expect(
            opened && *opened != nullptr,
            "OpenDatabase did not return an opened database"
            );
        if (opened && *opened != nullptr)
        {
            passed &= expect(
                (*opened)->schemaVersion().value_or(-1)
                    == DatabaseSchemaManager::LatestSchemaVersion,
                "OpenDatabase did not migrate its database"
                );
            passed &= expect(
                hasTable(**opened, "classes"),
                "OpenDatabase returned a database without the product schema"
                );
        }

        const auto blank = OpenDatabase::execute("  ");
        passed &= expect(
            !blank && blank.error().code == ErrorCode::InvalidArgument,
            "OpenDatabase accepted a blank path"
            );
    }

    return passed ? 0 : 1;
}
