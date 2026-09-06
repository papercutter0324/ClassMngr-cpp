#include "classmngr/engine/sqlite_database.h"

#if defined(_WIN32)
#include <winsqlite/winsqlite3.h>
#else
#include <sqlite3.h>
#endif

#include <algorithm>
#include <cctype>
#include <concepts>
#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>

namespace classmngr::engine
{
struct SqliteDatabaseImpl
{
    sqlite3* handle = nullptr;
    std::string databasePath;
};

namespace
{
constexpr int SqliteBusyTimeoutLimit =
    std::numeric_limits<int>::max();

bool isAsciiWhitespace(
    char character
    ) noexcept
{
    return std::isspace(static_cast<unsigned char>(character)) != 0;
}

bool isBlank(
    std::string_view text
    ) noexcept
{
    if (text.empty())
    {
        return true;
    }

    return std::all_of(
        text.begin(),
        text.end(),
        isAsciiWhitespace
        );
}

bool containsNull(
    std::string_view text
    ) noexcept
{
    return text.find('\0') != std::string_view::npos;
}

Error invalidArgument(
    std::string message
    )
{
    return {
        ErrorCode::InvalidArgument,
        std::move(message),
        std::nullopt
    };
}

Error databaseError(
    const SqliteDatabaseImpl& database,
    int nativeCode,
    std::string_view action,
    std::string_view detail = {}
    )
{
    std::string message(action);
    message += ": ";

    if (!detail.empty())
    {
        message += detail;
    }
    else if (database.handle != nullptr)
    {
        message += sqlite3_errmsg(database.handle);
    }
    else
    {
        message += sqlite3_errstr(nativeCode);
    }

    return {
        ErrorCode::Database,
        std::move(message),
        nativeCode
    };
}

Error schemaError(
    std::string message
    )
{
    return {
        ErrorCode::Schema,
        std::move(message),
        std::nullopt
    };
}

Status requireOpen(
    const SqliteDatabaseImpl& database
    )
{
    if (database.handle != nullptr)
    {
        return {};
    }

    return std::unexpected(Error{
        ErrorCode::Database,
        "SQLite database is not open.",
        std::nullopt
        });
}

Status validateSql(
    std::string_view sql
    )
{
    if (isBlank(sql))
    {
        return std::unexpected(invalidArgument(
            "SQLite statement must not be empty."
            ));
    }

    if (containsNull(sql))
    {
        return std::unexpected(invalidArgument(
            "SQLite statement must not contain an embedded null character."
            ));
    }

    if (sql.size() > static_cast<std::size_t>(SqliteBusyTimeoutLimit))
    {
        return std::unexpected(invalidArgument(
            "SQLite statement is too large."
            ));
    }

    return {};
}

bool hasNonWhitespace(
    const char* text
    ) noexcept
{
    if (text == nullptr)
    {
        return false;
    }

    while (*text != '\0')
    {
        if (!isAsciiWhitespace(*text))
        {
            return true;
        }
        ++text;
    }

    return false;
}

Result<sqlite3_stmt*> prepareSingleStatement(
    SqliteDatabaseImpl& database,
    std::string_view sql
    )
{
    const Status valid = validateSql(sql);
    if (!valid)
    {
        return std::unexpected(valid.error());
    }

    const std::string sqlText(sql);
    sqlite3_stmt* statement = nullptr;
    const char* tail = nullptr;
    const int result = sqlite3_prepare_v2(
        database.handle,
        sqlText.c_str(),
        static_cast<int>(sqlText.size()),
        &statement,
        &tail
        );
    if (result != SQLITE_OK)
    {
        if (statement != nullptr)
        {
            sqlite3_finalize(statement);
        }
        return std::unexpected(databaseError(
            database,
            result,
            "Preparing SQLite statement"
            ));
    }

    if (statement == nullptr)
    {
        return std::unexpected(invalidArgument(
            "SQLite statement must contain an executable command."
            ));
    }

    if (hasNonWhitespace(tail))
    {
        sqlite3_finalize(statement);
        return std::unexpected(invalidArgument(
            "Prepared SQLite statements must contain exactly one command."
            ));
    }

    return statement;
}

Status bindValue(
    SqliteDatabaseImpl& database,
    sqlite3_stmt* statement,
    int parameterIndex,
    const SqliteValue& value
    )
{
    int result = SQLITE_OK;
    std::visit(
        [statement, parameterIndex, &result](const auto& item)
        {
            using Value = std::decay_t<decltype(item)>;
            if constexpr (std::same_as<Value, std::monostate>)
            {
                result = sqlite3_bind_null(statement, parameterIndex);
            }
            else if constexpr (std::same_as<Value, std::int64_t>)
            {
                result = sqlite3_bind_int64(
                    statement,
                    parameterIndex,
                    item
                    );
            }
            else if constexpr (std::same_as<Value, double>)
            {
                result = sqlite3_bind_double(
                    statement,
                    parameterIndex,
                    item
                    );
            }
            else if constexpr (std::same_as<Value, std::string>)
            {
                if (item.size() > static_cast<std::size_t>(
                        std::numeric_limits<int>::max()
                        ))
                {
                    result = SQLITE_TOOBIG;
                    return;
                }

                result = sqlite3_bind_text(
                    statement,
                    parameterIndex,
                    item.data(),
                    static_cast<int>(item.size()),
                    SQLITE_TRANSIENT
                    );
            }
            else if constexpr (
                std::same_as<Value, std::vector<std::byte>>
                )
            {
                if (item.size() > static_cast<std::size_t>(
                        std::numeric_limits<int>::max()
                        ))
                {
                    result = SQLITE_TOOBIG;
                    return;
                }

                result = sqlite3_bind_blob(
                    statement,
                    parameterIndex,
                    item.empty()
                        ? nullptr
                        : static_cast<const void*>(item.data()),
                    static_cast<int>(item.size()),
                    SQLITE_TRANSIENT
                    );
            }
        },
        value
        );

    if (result == SQLITE_OK)
    {
        return {};
    }

    if (result == SQLITE_TOOBIG)
    {
        return std::unexpected(invalidArgument(
            "SQLite parameter is too large."
            ));
    }

    return std::unexpected(databaseError(
        database,
        result,
        "Binding SQLite parameter"
        ));
}

Status bindParameters(
    SqliteDatabaseImpl& database,
    sqlite3_stmt* statement,
    const SqliteParameters& parameters
    )
{
    const int expectedCount = sqlite3_bind_parameter_count(statement);
    if (parameters.size() != static_cast<std::size_t>(expectedCount))
    {
        return std::unexpected(invalidArgument(
            "SQLite parameter count does not match the prepared statement."
            ));
    }

    for (std::size_t index = 0; index < parameters.size(); ++index)
    {
        const Status bound = bindValue(
            database,
            statement,
            static_cast<int>(index + 1),
            parameters[index]
            );
        if (!bound)
        {
            return bound;
        }
    }

    return {};
}

SqliteValue valueAt(
    sqlite3_stmt* statement,
    int column
    )
{
    switch (sqlite3_column_type(statement, column))
    {
    case SQLITE_INTEGER:
        return sqlite3_column_int64(statement, column);
    case SQLITE_FLOAT:
        return sqlite3_column_double(statement, column);
    case SQLITE_TEXT:
    {
        const auto* text = sqlite3_column_text(statement, column);
        const int size = sqlite3_column_bytes(statement, column);
        if (text == nullptr || size <= 0)
        {
            return std::string();
        }

        return std::string(
            reinterpret_cast<const char*>(text),
            static_cast<std::size_t>(size)
            );
    }
    case SQLITE_BLOB:
    {
        const auto* data = sqlite3_column_blob(statement, column);
        const int size = sqlite3_column_bytes(statement, column);
        std::vector<std::byte> blob(static_cast<std::size_t>(std::max(size, 0)));
        if (data != nullptr && size > 0)
        {
            std::memcpy(blob.data(), data, static_cast<std::size_t>(size));
        }
        return blob;
    }
    case SQLITE_NULL:
    default:
        return std::monostate();
    }
}

Result<SqliteQueryResult> queryPrepared(
    SqliteDatabaseImpl& database,
    sqlite3_stmt* statement
    )
{
    SqliteQueryResult result;
    const int columnCount = sqlite3_column_count(statement);
    result.columnNames.reserve(static_cast<std::size_t>(columnCount));
    for (int column = 0; column < columnCount; ++column)
    {
        const char* name = sqlite3_column_name(statement, column);
        result.columnNames.emplace_back(name == nullptr ? "" : name);
    }

    while (true)
    {
        const int stepResult = sqlite3_step(statement);
        if (stepResult == SQLITE_ROW)
        {
            SqliteRow row;
            row.values.reserve(static_cast<std::size_t>(columnCount));
            for (int column = 0; column < columnCount; ++column)
            {
                row.values.push_back(valueAt(statement, column));
            }
            result.rows.push_back(std::move(row));
            continue;
        }

        if (stepResult == SQLITE_DONE)
        {
            return result;
        }

        return std::unexpected(databaseError(
            database,
            stepResult,
            "Stepping SQLite query"
            ));
    }
}

Result<int> readIntegerPragma(
    const SqliteDatabase& database,
    std::string_view queryText,
    std::string_view name
    )
{
    const auto query = database.query(queryText);
    if (!query)
    {
        return std::unexpected(query.error());
    }

    if (query->rows.size() != 1 || query->rows.front().values.size() != 1)
    {
        return std::unexpected(schemaError(
            "SQLite did not return a value for " + std::string(name) + "."
            ));
    }

    const auto* value = std::get_if<std::int64_t>(
        &query->rows.front().values.front()
        );
    if (value == nullptr
        || *value < 0
        || *value > std::numeric_limits<int>::max())
    {
        return std::unexpected(schemaError(
            "SQLite returned an invalid " + std::string(name) + "."
            ));
    }

    return static_cast<int>(*value);
}
} // namespace

SqliteDatabase::SqliteDatabase()
    : m_impl(std::make_unique<SqliteDatabaseImpl>())
{
}

SqliteDatabase::~SqliteDatabase()
{
    close();
}

Status SqliteDatabase::open(
    std::string_view databasePath,
    const SqliteOpenOptions& options
    )
{
    if (isBlank(databasePath))
    {
        return std::unexpected(invalidArgument(
            "SQLite database path must not be blank."
            ));
    }

    if (containsNull(databasePath))
    {
        return std::unexpected(invalidArgument(
            "SQLite database path must not contain an embedded null character."
            ));
    }

    if (options.busyTimeout.count() < 0
        || options.busyTimeout.count() > SqliteBusyTimeoutLimit)
    {
        return std::unexpected(invalidArgument(
            "SQLite busy timeout is outside the supported range."
            ));
    }

    close();

    sqlite3* handle = nullptr;
    const std::string path(databasePath);
    const int openResult = sqlite3_open_v2(
        path.c_str(),
        &handle,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr
        );
    if (openResult != SQLITE_OK)
    {
        std::string detail;
        if (handle != nullptr)
        {
            detail = sqlite3_errmsg(handle);
            sqlite3_close_v2(handle);
        }
        return std::unexpected(databaseError(
            *m_impl,
            openResult,
            "Opening SQLite database",
            detail
            ));
    }

    m_impl->handle = handle;
    m_impl->databasePath = path;

    sqlite3_extended_result_codes(m_impl->handle, 1);
    const int timeoutResult = sqlite3_busy_timeout(
        m_impl->handle,
        static_cast<int>(options.busyTimeout.count())
        );
    if (timeoutResult != SQLITE_OK)
    {
        const Error error = databaseError(
            *m_impl,
            timeoutResult,
            "Configuring SQLite busy timeout"
            );
        close();
        return std::unexpected(error);
    }

    if (options.enableForeignKeys)
    {
        const Status foreignKeys = execute(
            "PRAGMA foreign_keys = ON"
            );
        if (!foreignKeys)
        {
            const Error error = foreignKeys.error();
            close();
            return std::unexpected(error);
        }

        const Result<bool> enabled = foreignKeysEnabled();
        if (!enabled)
        {
            const Error error = enabled.error();
            close();
            return std::unexpected(error);
        }
        if (!*enabled)
        {
            close();
            return std::unexpected(databaseError(
                *m_impl,
                SQLITE_ERROR,
                "Enabling SQLite foreign keys",
                "SQLite did not enable foreign-key enforcement."
                ));
        }
    }

    return {};
}

void SqliteDatabase::close() noexcept
{
    if (m_impl == nullptr)
    {
        return;
    }

    if (m_impl->handle != nullptr)
    {
        sqlite3_close_v2(m_impl->handle);
        m_impl->handle = nullptr;
    }
    m_impl->databasePath.clear();
}

bool SqliteDatabase::isOpen() const noexcept
{
    return m_impl != nullptr && m_impl->handle != nullptr;
}

std::string_view SqliteDatabase::databasePath() const noexcept
{
    if (m_impl == nullptr)
    {
        return {};
    }

    return m_impl->databasePath;
}

Status SqliteDatabase::execute(
    std::string_view sql
    )
{
    if (m_impl == nullptr)
    {
        return std::unexpected(Error{
            ErrorCode::Internal,
            "SQLite database storage is unavailable.",
            std::nullopt
            });
    }

    const Status openStatus = requireOpen(*m_impl);
    if (!openStatus)
    {
        return openStatus;
    }

    const Status valid = validateSql(sql);
    if (!valid)
    {
        return valid;
    }

    const std::string sqlText(sql);
    char* errorMessage = nullptr;
    const int result = sqlite3_exec(
        m_impl->handle,
        sqlText.c_str(),
        nullptr,
        nullptr,
        &errorMessage
        );
    if (result == SQLITE_OK)
    {
        if (errorMessage != nullptr)
        {
            sqlite3_free(errorMessage);
        }
        return {};
    }

    std::string detail;
    if (errorMessage != nullptr)
    {
        detail = errorMessage;
        sqlite3_free(errorMessage);
    }
    return std::unexpected(databaseError(
        *m_impl,
        result,
        "Executing SQLite statement",
        detail
        ));
}

Status SqliteDatabase::execute(
    std::string_view sql,
    const SqliteParameters& parameters
    )
{
    if (m_impl == nullptr)
    {
        return std::unexpected(Error{
            ErrorCode::Internal,
            "SQLite database storage is unavailable.",
            std::nullopt
            });
    }

    const Status openStatus = requireOpen(*m_impl);
    if (!openStatus)
    {
        return openStatus;
    }

    const auto prepared = prepareSingleStatement(*m_impl, sql);
    if (!prepared)
    {
        return std::unexpected(prepared.error());
    }
    sqlite3_stmt* statement = *prepared;

    const Status bound = bindParameters(*m_impl, statement, parameters);
    if (!bound)
    {
        sqlite3_finalize(statement);
        return bound;
    }

    while (true)
    {
        const int result = sqlite3_step(statement);
        if (result == SQLITE_ROW)
        {
            continue;
        }
        if (result == SQLITE_DONE)
        {
            const int finalizeResult = sqlite3_finalize(statement);
            if (finalizeResult != SQLITE_OK)
            {
                return std::unexpected(databaseError(
                    *m_impl,
                    finalizeResult,
                    "Finalizing SQLite statement"
                    ));
            }
            return {};
        }

        sqlite3_finalize(statement);
        return std::unexpected(databaseError(
            *m_impl,
            result,
            "Stepping SQLite statement"
            ));
    }
}

Result<SqliteQueryResult> SqliteDatabase::query(
    std::string_view sql,
    const SqliteParameters& parameters
    ) const
{
    if (m_impl == nullptr)
    {
        return std::unexpected(Error{
            ErrorCode::Internal,
            "SQLite database storage is unavailable.",
            std::nullopt
            });
    }

    const Status openStatus = requireOpen(*m_impl);
    if (!openStatus)
    {
        return std::unexpected(openStatus.error());
    }

    // The C API only mutates the statement and connection state, not the
    // database object or its path, so this private implementation is safe to
    // use for const queries.
    const auto prepared = prepareSingleStatement(
        *m_impl,
        sql
        );
    if (!prepared)
    {
        return std::unexpected(prepared.error());
    }
    sqlite3_stmt* statement = *prepared;

    const Status bound = bindParameters(*m_impl, statement, parameters);
    if (!bound)
    {
        sqlite3_finalize(statement);
        return std::unexpected(bound.error());
    }

    const auto result = queryPrepared(*m_impl, statement);
    const int finalizeResult = sqlite3_finalize(statement);
    if (!result)
    {
        return std::unexpected(result.error());
    }
    if (finalizeResult != SQLITE_OK)
    {
        return std::unexpected(databaseError(
            *m_impl,
            finalizeResult,
            "Finalizing SQLite query"
            ));
    }

    return result;
}

Result<SqliteTransaction> SqliteDatabase::beginTransaction()
{
    SqliteTransaction transaction(*this);
    if (!transaction.started())
    {
        if (transaction.startError())
        {
            return std::unexpected(*transaction.startError());
        }
        return std::unexpected(Error{
            ErrorCode::Internal,
            "SQLite transaction failed without an error.",
            std::nullopt
            });
    }

    return std::move(transaction);
}

Result<int> SqliteDatabase::schemaVersion() const
{
    return readIntegerPragma(
        *this,
        "PRAGMA user_version",
        "database schema version"
        );
}

Status SqliteDatabase::setSchemaVersion(
    int version
    )
{
    if (version < 0)
    {
        return std::unexpected(invalidArgument(
            "SQLite schema version must not be negative."
            ));
    }

    return execute(
        "PRAGMA user_version = " + std::to_string(version)
        );
}

Result<bool> SqliteDatabase::foreignKeysEnabled() const
{
    const auto version = readIntegerPragma(
        *this,
        "PRAGMA foreign_keys",
        "foreign-key setting"
        );
    if (!version)
    {
        return std::unexpected(version.error());
    }

    if (*version > 1)
    {
        return std::unexpected(schemaError(
            "SQLite returned an invalid foreign-key setting."
            ));
    }

    return *version == 1;
}

void SqliteDatabase::rollbackTransactionNoThrow() noexcept
{
    if (m_impl == nullptr || m_impl->handle == nullptr)
    {
        return;
    }

    char* errorMessage = nullptr;
    sqlite3_exec(
        m_impl->handle,
        "ROLLBACK",
        nullptr,
        nullptr,
        &errorMessage
        );
    if (errorMessage != nullptr)
    {
        sqlite3_free(errorMessage);
    }
}

SqliteTransaction::SqliteTransaction(
    SqliteDatabase& database
    )
    : m_database(&database)
{
    const Status started = database.execute("BEGIN IMMEDIATE");
    if (!started)
    {
        m_startError = started.error();
        return;
    }

    m_started = true;
}

SqliteTransaction::~SqliteTransaction()
{
    if (m_started && !m_finished)
    {
        rollback();
    }
}

SqliteTransaction::SqliteTransaction(
    SqliteTransaction&& other
    ) noexcept
    : m_database(std::exchange(other.m_database, nullptr))
    , m_startError(std::move(other.m_startError))
    , m_started(std::exchange(other.m_started, false))
    , m_finished(std::exchange(other.m_finished, true))
{
}

SqliteTransaction& SqliteTransaction::operator=(
    SqliteTransaction&& other
    ) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    rollback();
    m_database = std::exchange(other.m_database, nullptr);
    m_startError = std::move(other.m_startError);
    m_started = std::exchange(other.m_started, false);
    m_finished = std::exchange(other.m_finished, true);
    return *this;
}

bool SqliteTransaction::started() const noexcept
{
    return m_started;
}

const std::optional<Error>& SqliteTransaction::startError() const noexcept
{
    return m_startError;
}

Status SqliteTransaction::commit()
{
    if (!m_started || m_database == nullptr)
    {
        return std::unexpected(invalidArgument(
            "SQLite transaction was not started."
            ));
    }
    if (m_finished)
    {
        return std::unexpected(invalidArgument(
            "SQLite transaction has already finished."
            ));
    }

    const Status committed = m_database->execute("COMMIT");
    if (committed)
    {
        m_finished = true;
    }
    return committed;
}

void SqliteTransaction::rollback() noexcept
{
    if (!m_started || m_finished || m_database == nullptr)
    {
        return;
    }

    m_database->rollbackTransactionNoThrow();
    m_finished = true;
}

} // namespace classmngr::engine
