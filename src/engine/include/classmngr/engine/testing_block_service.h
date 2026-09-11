#pragma once

#include "classmngr/engine/result.h"
#include "classmngr/engine/testing_block.h"

#include <string_view>
#include <vector>

namespace classmngr::engine
{

class SqliteDatabase;

class TestingBlockService final
{
public:
    explicit TestingBlockService(
        SqliteDatabase& database
        );

    [[nodiscard]] Result<std::vector<TestingAssignment>> listAssignments();

    [[nodiscard]] Result<std::vector<TestingBlock>> listBlocks();

    [[nodiscard]] Status saveBlock(
        std::string_view day,
        std::string_view startTime,
        std::string_view room,
        bool replaceExisting = false
        );

    [[nodiscard]] Status assignClass(
        std::string_view day,
        std::string_view startTime,
        int classId,
        bool replaceExisting = false
        );

    [[nodiscard]] Status deleteAssignment(
        std::string_view day,
        std::string_view startTime
        );

    [[nodiscard]] Status deleteBlock(
        std::string_view day,
        std::string_view startTime
        );

    [[nodiscard]] Status clearAssignments();

    [[nodiscard]] Status clearBlocks();

private:
    SqliteDatabase& m_database;
};

} // namespace classmngr::engine
