#include "classmngr/engine/application_settings_service.h"

#include "classmngr/engine/sqlite_database.h"

#include <string>
#include <utility>
#include <variant>

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

SettingValue fromSqliteValue(
    const SqliteValue& value
    )
{
    return std::visit(
        [](const auto& item) -> SettingValue
        {
            return item;
        },
        value
        );
}

SqliteValue toSqliteValue(
    const SettingValue& value
    )
{
    return std::visit(
        [](const auto& item) -> SqliteValue
        {
            return item;
        },
        value
        );
}

std::string_view valueExpression(
    const SettingValue& value
    ) noexcept
{
    if (std::holds_alternative<std::int64_t>(value))
    {
        return "CAST(? AS INTEGER)";
    }
    if (std::holds_alternative<double>(value))
    {
        return "CAST(? AS REAL)";
    }
    if (std::holds_alternative<std::vector<std::byte>>(value))
    {
        return "CAST(? AS BLOB)";
    }

    return "?";
}

Error settingContext(
    Error cause,
    std::string_view key
    )
{
    cause.message = "setting key '"
        + std::string(key)
        + "': "
        + cause.message;
    return cause;
}

Result<SettingValue> settingValueFromRow(
    const SqliteRow& row,
    std::string_view requestedKey
    )
{
    if (row.values.size() != 2)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an unexpected app_settings row shape."
            ));
    }

    const auto* key = std::get_if<std::string>(&row.values[0]);
    if (key == nullptr)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned a non-text app_settings key."
            ));
    }
    if (*key != requestedKey)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an unexpected app_settings key."
            ));
    }

    return fromSqliteValue(row.values[1]);
}
} // namespace

ApplicationSettingsService::ApplicationSettingsService(
    SqliteDatabase& database
    )
    : m_database(database)
{
}

Status ApplicationSettingsService::save(
    std::string_view key,
    const SettingValue& value
    )
{
    const std::string sql =
        "INSERT INTO app_settings (key, value) VALUES (?, "
        + std::string(valueExpression(value))
        + ") ON CONFLICT(key) DO UPDATE SET value=excluded.value";
    return m_database.execute(
        sql,
        SqliteParameters{
            SqliteValue{std::string(key)},
            toSqliteValue(value)
        }
        );
}

Status ApplicationSettingsService::saveBatch(
    const ApplicationSettings& settings
    )
{
    if (settings.empty())
    {
        return {};
    }

    Result<SqliteTransaction> transactionResult =
        m_database.beginTransaction();
    if (!transactionResult)
    {
        return std::unexpected(transactionResult.error());
    }
    SqliteTransaction transaction = std::move(*transactionResult);

    for (const auto& [key, value] : settings)
    {
        const Status saved = save(key, value);
        if (!saved)
        {
            return std::unexpected(settingContext(saved.error(), key));
        }
    }

    return transaction.commit();
}

Result<SettingValue> ApplicationSettingsService::load(
    std::string_view key
    )
{
    const Result<SqliteQueryResult> rows = m_database.query(
        "SELECT key, value FROM app_settings WHERE key=?",
        SqliteParameters{
            SqliteValue{std::string(key)}
        }
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }

    if (rows->columnNames.size() != 2
        || rows->columnNames[0] != "key"
        || rows->columnNames[1] != "value")
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an unexpected app_settings column shape."
            ));
    }
    if (rows->rows.size() > 1)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned multiple app_settings rows for one key."
            ));
    }
    if (rows->rows.empty())
    {
        return std::monostate{};
    }

    return settingValueFromRow(rows->rows.front(), key);
}

} // namespace classmngr::engine
