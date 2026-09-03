#pragma once

#include "classmngr/engine/calendar_event.h"

#include <string>
#include <vector>

namespace classmngr::engine
{

struct CalendarImportCell
{
    int row = 0;
    int column = 0;
    int style = 0;
    std::string value;
    std::string note;
};

struct CalendarImportStyle
{
    std::string fillColor;
    std::string fontColor;
};

struct CalendarImportWorkbook
{
    std::vector<CalendarImportCell> cells;
    std::vector<CalendarImportStyle> styles;
};

struct CalendarImportResult
{
    std::vector<CalendarEvent> events;
    int skippedCount = 0;
};

struct CalendarEventImportSummary
{
    int importedCount = 0;
    int skippedCount = 0;
};

class CalendarEventImportService final
{
public:
    [[nodiscard]] static CalendarImportResult parse(
        const CalendarImportWorkbook& workbook,
        const std::vector<std::string>& campusCodes = {}
        );

    [[nodiscard]] static std::string importSignature(
        const CalendarEvent& event
        );
};

} // namespace classmngr::engine
