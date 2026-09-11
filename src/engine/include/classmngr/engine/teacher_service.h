#pragma once

#include "classmngr/engine/result.h"
#include "classmngr/engine/teacher.h"

#include <string_view>
#include <vector>

namespace classmngr::engine
{

class SqliteDatabase;

// Validated teacher CRUD is an engine use case.  Presentation adapters should
// depend on this boundary instead of duplicating SQL or teacher rules.
class TeacherService final
{
public:
    explicit TeacherService(
        SqliteDatabase& database
        );

    [[nodiscard]] Result<int> create(
        const Teacher& teacher
        );

    [[nodiscard]] Result<int> save(
        const Teacher& teacher
        );

    [[nodiscard]] Status update(
        const Teacher& teacher
        );

    [[nodiscard]] Result<Teacher> get(
        int teacherId
        );

    [[nodiscard]] Result<std::vector<Teacher>> list();

    [[nodiscard]] Status remove(
        int teacherId
        );

private:
    SqliteDatabase& m_database;
};

} // namespace classmngr::engine
