#pragma once

#include "classmngr/engine/result.h"
#include "classmngr/engine/schedule_import.h"

#include <filesystem>
#include <functional>

namespace classmngr::engine
{

using ScheduleWorkbookCancellation = std::function<bool()>;

// File codecs are presentation/platform adapters.  They return the same
// renderer-neutral import model consumed by ScheduleImportService and never
// expose Qt, OpenXLSX, ZIP, XML, or WinRT types to callers.
class ScheduleWorkbookReader
{
public:
    virtual ~ScheduleWorkbookReader() = default;

    [[nodiscard]] virtual Result<ScheduleImportWorkbook> read(
        const std::filesystem::path& file,
        ScheduleImportKind kind,
        const ScheduleWorkbookCancellation& isCancelled = {}
        ) const = 0;
};

} // namespace classmngr::engine
