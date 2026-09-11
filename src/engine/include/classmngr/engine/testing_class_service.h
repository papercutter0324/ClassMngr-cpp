#pragma once

#include "classmngr/engine/result.h"
#include "classmngr/engine/testing_class.h"

#include <string_view>
#include <vector>

namespace classmngr::engine
{

class SqliteDatabase;

class TestingClassService final
{
public:
    explicit TestingClassService(
        SqliteDatabase& database
        );

    [[nodiscard]] Result<int> create(
        const TestingClass& testingClass,
        std::string_view assignmentDay = {},
        std::string_view assignmentStartTime = {}
        );

    [[nodiscard]] Status update(
        const TestingClass& testingClass
        );

    [[nodiscard]] Result<TestingClass> get(
        int classId
        );

    [[nodiscard]] Result<std::vector<TestingClass>> list();

    [[nodiscard]] Status remove(
        int classId
        );

    [[nodiscard]] Result<bool> isTestingClass(
        int classId
        );

private:
    SqliteDatabase& m_database;
};

} // namespace classmngr::engine
