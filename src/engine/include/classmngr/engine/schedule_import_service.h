#pragma once

#include "classmngr/engine/result.h"
#include "classmngr/engine/schedule_import.h"

#include <functional>

namespace classmngr::engine
{

class SqliteDatabase;
using ScheduleImportCancellation = std::function<bool()>;

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
        ScheduleImportKind kind,
        const ScheduleImportCancellation& isCancelled = {}
        );

    [[nodiscard]] Status validateImport(
        const ScheduleImportPlan& plan,
        const ScheduleImportCancellation& isCancelled = {}
        );

    [[nodiscard]] Result<ScheduleImportSummary> importSchedule(
        const ScheduleImportPlan& plan,
        const ScheduleImportCancellation& isCancelled = {}
        );

private:
    SqliteDatabase& m_database;
};

} // namespace classmngr::engine
