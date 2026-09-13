#pragma once

#include "classmngr/engine/result.h"
#include "classmngr/engine/schedule_workbook_layout.h"
#include "classmngr/engine/schedule_workbook_reader.h"

namespace classmngr::engine
{

// Interprets a value-owned workbook layout according to the schedule template
// rules.  The implementation is deliberately independent of the file codec;
// Qt and OpenXLSX adapters can therefore share one semantic path.
class ScheduleWorkbookInterpreter final
{
public:
    [[nodiscard]] static Result<ScheduleImportWorkbook> interpret(
        const ScheduleWorkbookLayout& layout,
        ScheduleImportKind kind,
        const ScheduleWorkbookCancellation& isCancelled = {}
        );
};

} // namespace classmngr::engine
