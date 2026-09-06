#include "classmngr/engine/sqlite_database.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace
{
using classmngr::engine::ErrorCode;
using classmngr::engine::SqliteDatabase;
using classmngr::engine::SqliteParameters;
using classmngr::engine::SqliteValue;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineSqliteDatabaseTests: "
              << message
              << '\n';
    return false;
}

const SqliteValue* valueAt(
    const classmngr::engine::SqliteQueryResult& result,
    std::size_t row,
    std::size_t column
    )
{
    if (row >= result.rows.size()
        || column >= result.rows[row].values.size())
    {
        return nullptr;
    }

    return &result.rows[row].values[column];
}

bool hasCount(
    SqliteDatabase& database,
    std::int64_t expected
    )
{
    const auto result = database.query("SELECT COUNT(*) FROM records");
    if (!result || result->rows.size() != 1)
    {
        return false;
    }

    const auto* value = valueAt(*result, 0, 0);
    const auto* count = value == nullptr
        ? nullptr
        : std::get_if<std::int64_t>(value);
    return count != nullptr && *count == expected;
}
}

int main()
{
    bool passed = true;
    SqliteDatabase database;

    const auto unopenedVersion = database.schemaVersion();
    passed &= expect(
        !unopenedVersion
            && unopenedVersion.error().code == ErrorCode::Database,
        "unopened database did not return a database error"
        );

    passed &= expect(
        !database.open("   "),
        "blank database path was accepted"
        );

    passed &= expect(
        database.open(":memory:").has_value(),
        "in-memory database did not open"
        );
    passed &= expect(
        database.isOpen() && database.databasePath() == ":memory:",
        "database identity was not retained"
        );

    const auto foreignKeys = database.foreignKeysEnabled();
    passed &= expect(
        foreignKeys && *foreignKeys,
        "foreign-key enforcement was not enabled"
        );

    const auto initialVersion = database.schemaVersion();
    passed &= expect(
        initialVersion && *initialVersion == 0,
        "new database did not start at schema version zero"
        );

    passed &= expect(
        database.execute(
            "CREATE TABLE records ("
            "id INTEGER PRIMARY KEY, "
            "name TEXT NOT NULL, "
            "score REAL NOT NULL, "
            "payload BLOB, "
            "note TEXT"
            ")"
            ).has_value(),
        "table creation failed"
        );

    const std::vector<std::byte> payload{
        std::byte{0x01},
        std::byte{0x7f},
        std::byte{0xff}
    };
    const SqliteParameters firstRecord{
        std::int64_t{1},
        std::string("김민준"),
        98.5,
        payload,
        std::monostate{}
    };
    passed &= expect(
        database.execute(
            "INSERT INTO records (id, name, score, payload, note) "
            "VALUES (?, ?, ?, ?, ?)",
            firstRecord
            ).has_value(),
        "prepared insert failed"
        );

    const auto rows = database.query(
        "SELECT id, name, score, payload, note FROM records WHERE id = ?",
        SqliteParameters{std::int64_t{1}}
        );
    passed &= expect(
        rows && rows->columnNames == std::vector<std::string>{
            "id", "name", "score", "payload", "note"
        },
        "typed query column names changed"
        );
    passed &= expect(
        rows && rows->rows.size() == 1,
        "typed query did not return its inserted row"
        );

    if (rows && rows->rows.size() == 1)
    {
        const auto* id = valueAt(*rows, 0, 0);
        const auto* name = valueAt(*rows, 0, 1);
        const auto* score = valueAt(*rows, 0, 2);
        const auto* returnedPayload = valueAt(*rows, 0, 3);
        const auto* note = valueAt(*rows, 0, 4);
        passed &= expect(
            id != nullptr
                && std::get_if<std::int64_t>(id) != nullptr
                && *std::get_if<std::int64_t>(id) == 1,
            "integer column was not mapped"
            );
        passed &= expect(
            name != nullptr
                && std::get_if<std::string>(name) != nullptr
                && *std::get_if<std::string>(name) == "김민준",
            "UTF-8 text column was not mapped"
            );
        passed &= expect(
            score != nullptr
                && std::get_if<double>(score) != nullptr
                && *std::get_if<double>(score) == 98.5,
            "real column was not mapped"
            );
        passed &= expect(
            returnedPayload != nullptr
                && std::get_if<std::vector<std::byte>>(returnedPayload) != nullptr
                && *std::get_if<std::vector<std::byte>>(returnedPayload) == payload,
            "blob column was not mapped"
            );
        passed &= expect(
            note != nullptr
                && std::get_if<std::monostate>(note) != nullptr,
            "null column was not mapped"
            );
    }

    const auto mismatchedParameters = database.execute(
        "INSERT INTO records (id, name, score) VALUES (?, ?, ?)",
        SqliteParameters{std::int64_t{2}}
        );
    passed &= expect(
        !mismatchedParameters
            && mismatchedParameters.error().code == ErrorCode::InvalidArgument,
        "parameter count mismatch was not rejected"
        );

    const auto schemaUpdated = database.setSchemaVersion(4);
    const auto updatedVersion = database.schemaVersion();
    passed &= expect(
        schemaUpdated && updatedVersion && *updatedVersion == 4,
        "schema version round trip failed"
        );
    passed &= expect(
        !database.setSchemaVersion(-1),
        "negative schema version was accepted"
        );

    {
        auto transaction = database.beginTransaction();
        passed &= expect(
            transaction && transaction->started(),
            "rollback transaction did not start"
            );
        if (transaction)
        {
            passed &= expect(
                database.execute(
                    "INSERT INTO records (id, name, score) VALUES (?, ?, ?)",
                    SqliteParameters{
                        std::int64_t{2},
                        std::string("Rollback"),
                        1.0
                    }
                    ).has_value(),
                "transaction insert failed"
                );
            transaction->rollback();
        }
    }
    passed &= expect(
        hasCount(database, 1),
        "rolled-back transaction changed persisted rows"
        );

    {
        auto transaction = database.beginTransaction();
        passed &= expect(
            transaction && transaction->started(),
            "commit transaction did not start"
            );
        if (transaction)
        {
            passed &= expect(
                database.execute(
                    "INSERT INTO records (id, name, score) VALUES (?, ?, ?)",
                    SqliteParameters{
                        std::int64_t{3},
                        std::string("Commit"),
                        2.0
                    }
                    ).has_value(),
                "commit transaction insert failed"
                );
            passed &= expect(
                transaction->commit().has_value(),
                "commit transaction did not commit"
                );
        }
    }
    passed &= expect(
        hasCount(database, 2),
        "committed transaction did not persist its row"
        );

    database.close();
    passed &= expect(
        !database.isOpen() && database.databasePath().empty(),
        "database close did not clear the connection"
        );

    return passed ? 0 : 1;
}
