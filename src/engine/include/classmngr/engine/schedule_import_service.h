#pragma once

#include "classmngr/engine/result.h"
#include "classmngr/engine/schedule_import.h"

namespace classmngr::engine
{

class SqliteDatabase;

// Workbook/file codecs belong to presentation adapters.  This service owns
// the renderer-neutral schedule-import preview, plan validation, conflict
// projection, and transactional database snapshot application.
class ScheduleImportService final
{
public:
    explicit ScheduleImportService(
        SqliteDatabase& database
        );

    [[nodiscard]] Result<ScheduleImportPreview> previewImport(
        const ScheduleImportUserBlock& user,
        ScheduleImportKind kind
        );

    [[nodiscard]] Result<ScheduleImportSummary> importSchedule(
        const ScheduleImportPlan& plan
        );

private:
    SqliteDatabase& m_database;
};

} // namespace classmngr::engine
