#pragma once

#include "classmngr/engine/schedule_workbook_reader.h"

#include <memory>

namespace classmngr::winui
{

class ScheduleWorkbookOpenXLSXReader final
    : public classmngr::engine::ScheduleWorkbookReader
{
public:
    [[nodiscard]] classmngr::engine::Result<
        classmngr::engine::ScheduleImportWorkbook> read(
            const std::filesystem::path& file,
            classmngr::engine::ScheduleImportKind kind,
            const classmngr::engine::ScheduleWorkbookCancellation& isCancelled = {}
            ) const override;
};

[[nodiscard]] std::unique_ptr<classmngr::engine::ScheduleWorkbookReader>
makeScheduleWorkbookReader();

} // namespace classmngr::winui
