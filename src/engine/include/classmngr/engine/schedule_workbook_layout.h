#pragma once

#include "classmngr/engine/schedule_import.h"

#include <string>
#include <vector>

namespace classmngr::engine
{

// A codec-neutral snapshot of the workbook facts used by schedule-format
// interpretation.  Adapters must materialize values here before their
// workbook document is closed; no object in this model refers to a library
// document, XML node, ZIP entry, or UI object.
struct ScheduleWorkbookLayoutCell
{
    int row = 0;
    int column = 0;
    int style = 0;
    std::string value;
    std::string note;
};

struct ScheduleWorkbookLayoutStyle
{
    std::string fillColor;
    std::string fontColor;
    bool filled = false;
    bool bold = false;
};

struct ScheduleWorkbookLayoutRange
{
    int firstRow = 0;
    int firstColumn = 0;
    int lastRow = 0;
    int lastColumn = 0;

    [[nodiscard]] bool contains(
        int row,
        int column
        ) const noexcept
    {
        return row >= firstRow
            && row <= lastRow
            && column >= firstColumn
            && column <= lastColumn;
    }
};

struct ScheduleWorkbookLayoutSheet
{
    std::string name;
    bool visible = true;
    std::vector<ScheduleWorkbookLayoutCell> cells;
    std::vector<ScheduleWorkbookLayoutRange> mergedRanges;
};

struct ScheduleWorkbookLayout
{
    std::vector<ScheduleWorkbookLayoutStyle> styles;
    std::vector<ScheduleWorkbookLayoutSheet> sheets;
    std::vector<ScheduleImportDiagnostic> diagnostics;
};

} // namespace classmngr::engine
