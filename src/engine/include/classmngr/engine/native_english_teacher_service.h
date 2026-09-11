#pragma once

#include "classmngr/engine/native_english_teacher.h"
#include "classmngr/engine/result.h"

#include <vector>

namespace classmngr::engine
{

class SqliteDatabase;

class NativeEnglishTeacherService final
{
public:
    explicit NativeEnglishTeacherService(
        SqliteDatabase& database
        );

    [[nodiscard]] Result<std::vector<NativeEnglishTeacher>> list() const;

    [[nodiscard]] Status saveDirectory(
        const std::vector<NativeEnglishTeacher>& teachers,
        const std::vector<int>& deletedIds
        );

private:
    SqliteDatabase& m_database;
};

} // namespace classmngr::engine
