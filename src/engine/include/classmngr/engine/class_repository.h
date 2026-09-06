#pragma once

#include "classmngr/engine/classroom.h"
#include "classmngr/engine/result.h"

#include <string_view>
#include <vector>

namespace classmngr::engine
{

class SqliteDatabase;

class ClassRepository final
{
public:
    explicit ClassRepository(
        SqliteDatabase& database
        );

    [[nodiscard]] Result<int> create(
        std::string_view name
        );

    [[nodiscard]] Result<std::vector<Classroom>> list();

    [[nodiscard]] Result<Classroom> get(
        int classId
        );

    [[nodiscard]] Status rename(
        int classId,
        std::string_view name
        );

    [[nodiscard]] Status remove(
        int classId
        );

private:
    SqliteDatabase& m_database;
};

} // namespace classmngr::engine
