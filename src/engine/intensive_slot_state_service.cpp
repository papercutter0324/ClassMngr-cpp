#include "classmngr/engine/intensive_slot_state_service.h"

#include "classmngr/engine/sqlite_database.h"

#include <optional>
#include <string>
#include <string_view>
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

Result<std::string> textFromValue(
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
        "SQLite returned a non-text intensive slot state "
            + std::string(column) + " value."
        ));
}

Result<IntensiveSlotState> stateFromRow(
    const SqliteRow& row
    )
{
    if (row.values.size() != 3)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an unexpected intensive slot state row shape."
            ));
    }

    const Result<std::string> day = textFromValue(
        row.values[0],
        "day"
        );
    const Result<std::string> startTime = textFromValue(
        row.values[1],
        "start_time"
        );
    const Result<std::string> state = textFromValue(
        row.values[2],
        "state"
        );
    if (!day)
    {
        return std::unexpected(day.error());
    }
    if (!startTime)
    {
        return std::unexpected(startTime.error());
    }
    if (!state)
    {
        return std::unexpected(state.error());
    }

    return IntensiveSlotState{
        *day,
        *startTime,
        *state
    };
}
} // namespace

IntensiveSlotStateService::IntensiveSlotStateService(
    SqliteDatabase& database
    )
    : m_database(database)
{
}

Result<std::vector<IntensiveSlotState>> IntensiveSlotStateService::list() const
{
    const auto rows = m_database.query(
        "SELECT day, start_time, state "
        "FROM intensive_slot_states "
        "ORDER BY day, start_time"
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }

    std::vector<IntensiveSlotState> states;
    states.reserve(rows->rows.size());
    for (const SqliteRow& row : rows->rows)
    {
        const Result<IntensiveSlotState> state = stateFromRow(row);
        if (!state)
        {
            return std::unexpected(state.error());
        }
        states.push_back(*state);
    }

    return states;
}

Status IntensiveSlotStateService::save(
    std::string_view day,
    std::string_view startTime,
    std::string_view state,
    std::string_view defaultState
    )
{
    if (state == defaultState)
    {
        return m_database.execute(
            "DELETE FROM intensive_slot_states "
            "WHERE day=? AND start_time=?",
            SqliteParameters{
                SqliteValue{std::string(day)},
                SqliteValue{std::string(startTime)}
            }
            );
    }

    return m_database.execute(
        "INSERT INTO intensive_slot_states (day, start_time, state) "
        "VALUES (?, ?, ?) "
        "ON CONFLICT(day, start_time) "
        "DO UPDATE SET state=excluded.state",
        SqliteParameters{
            SqliteValue{std::string(day)},
            SqliteValue{std::string(startTime)},
            SqliteValue{std::string(state)}
        }
        );
}

} // namespace classmngr::engine
