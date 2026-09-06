#pragma once

#include "classmngr/engine/intensive_slot_state.h"
#include "classmngr/engine/result.h"

#include <string_view>
#include <vector>

namespace classmngr::engine
{

class SqliteDatabase;

class IntensiveSlotStateService final
{
public:
    explicit IntensiveSlotStateService(
        SqliteDatabase& database
        );

    [[nodiscard]] Result<std::vector<IntensiveSlotState>> list() const;

    [[nodiscard]] Status save(
        std::string_view day,
        std::string_view startTime,
        std::string_view state,
        std::string_view defaultState = "essay"
        );

private:
    SqliteDatabase& m_database;
};

} // namespace classmngr::engine
