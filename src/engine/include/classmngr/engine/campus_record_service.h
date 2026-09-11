#pragma once

#include "classmngr/engine/campus_record.h"
#include "classmngr/engine/result.h"

#include <vector>

namespace classmngr::engine
{

class SqliteDatabase;

class CampusRecordService final
{
public:
    explicit CampusRecordService(
        SqliteDatabase& database
        );

    [[nodiscard]] Result<int> create(
        const CampusRecord& campus
        );

    [[nodiscard]] Result<int> save(
        const CampusRecord& campus
        );

    [[nodiscard]] Status update(
        const CampusRecord& campus
        );

    [[nodiscard]] Result<CampusRecord> get(
        int campusId
        );

    [[nodiscard]] Result<std::vector<CampusRecord>> list();

    [[nodiscard]] Status remove(
        int campusId
        );

private:
    SqliteDatabase& m_database;
};

} // namespace classmngr::engine
