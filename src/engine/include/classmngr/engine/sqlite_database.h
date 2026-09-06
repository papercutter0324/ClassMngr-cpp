#pragma once

#include "classmngr/engine/result.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace classmngr::engine
{

struct SqliteDatabaseImpl;

struct SqliteOpenOptions
{
    std::chrono::milliseconds busyTimeout{5000};
    bool enableForeignKeys = true;
};

using SqliteValue = std::variant<
    std::monostate,
    std::int64_t,
    double,
    std::string,
    std::vector<std::byte>
    >;

using SqliteParameters = std::vector<SqliteValue>;

struct SqliteRow
{
    std::vector<SqliteValue> values;
};

struct SqliteQueryResult
{
    std::vector<std::string> columnNames;
    std::vector<SqliteRow> rows;
};

class SqliteDatabase;

class SqliteTransaction final
{
public:
    explicit SqliteTransaction(
        SqliteDatabase& database
        );
    ~SqliteTransaction();

    SqliteTransaction(const SqliteTransaction&) = delete;
    SqliteTransaction& operator=(const SqliteTransaction&) = delete;

    SqliteTransaction(SqliteTransaction&& other) noexcept;
    SqliteTransaction& operator=(SqliteTransaction&& other) noexcept;

    [[nodiscard]] bool started() const noexcept;
    [[nodiscard]] const std::optional<Error>& startError() const noexcept;

    [[nodiscard]] Status commit();
    void rollback() noexcept;

private:
    SqliteDatabase* m_database = nullptr;
    std::optional<Error> m_startError;
    bool m_started = false;
    bool m_finished = false;
};

class SqliteDatabase final
{
public:
    SqliteDatabase();
    ~SqliteDatabase();

    SqliteDatabase(const SqliteDatabase&) = delete;
    SqliteDatabase& operator=(const SqliteDatabase&) = delete;

    [[nodiscard]] Status open(
        std::string_view databasePath,
        const SqliteOpenOptions& options = {}
        );
    void close() noexcept;

    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] std::string_view databasePath() const noexcept;

    [[nodiscard]] Status execute(
        std::string_view sql
        );
    [[nodiscard]] Status execute(
        std::string_view sql,
        const SqliteParameters& parameters
        );
    [[nodiscard]] Result<SqliteQueryResult> query(
        std::string_view sql,
        const SqliteParameters& parameters = {}
        ) const;

    [[nodiscard]] Result<SqliteTransaction> beginTransaction();

    [[nodiscard]] Result<int> schemaVersion() const;
    [[nodiscard]] Status setSchemaVersion(int version);
    [[nodiscard]] Result<bool> foreignKeysEnabled() const;

private:
    mutable std::unique_ptr<SqliteDatabaseImpl> m_impl;

    void rollbackTransactionNoThrow() noexcept;

    friend class SqliteTransaction;
};

} // namespace classmngr::engine
