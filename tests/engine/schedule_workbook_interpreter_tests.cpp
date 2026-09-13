#include "classmngr/engine/schedule_workbook_interpreter.h"

#include <cassert>
#include <utility>

namespace
{
std::string teacher()
{
    return "\xED\x99\x8D\xEC\x8A\xB9\xED\x98\x84";
}

class LayoutFixture
{
public:
    LayoutFixture()
    {
        layout.styles = {
            {},
            {"FF6D9EEB", "FF000000", true, false}
        };
        classmngr::engine::ScheduleWorkbookLayoutSheet sheet;
        sheet.name = "Current";
        sheet.visible = true;
        sheet.cells = {
            {1, 1, 0, "Alice", ""},
            {1, 2, 0, "MON", ""},
            {1, 3, 0, "TUE", ""},
            {1, 4, 0, "WED", ""},
            {1, 5, 0, "THU", ""},
            {1, 6, 0, "FRI", ""},
            {2, 1, 0, "4:00~4:55", ""},
            {2, 2, 1, teacher() + " (413)\nE5-Zeus", ""},
            {3, 1, 0, "5:00~5:55", ""}
        };
        sheet.mergedRanges = {{2, 2, 3, 2}};
        layout.sheets.push_back(std::move(sheet));
    }

    classmngr::engine::ScheduleWorkbookLayout layout;
};
}

int main()
{
    using namespace classmngr::engine;

    LayoutFixture fixture;
    const auto parsed = ScheduleWorkbookInterpreter::interpret(
        fixture.layout,
        ScheduleImportKind::Normal
        );
    assert(parsed.has_value());
    assert(parsed->sheets.size() == 1);
    assert(parsed->sheets.front().users.size() == 1);
    const auto& candidate = parsed->sheets.front().users.front().classes.front();
    assert(candidate.teacherKr == teacher());
    assert(candidate.rooms.size() == 1 && candidate.rooms.front() == "413");
    assert(candidate.importedColors.size() == 1
           && candidate.importedColors.front() == "#6D9EEB");
    assert(candidate.times.size() == 1);
    assert(candidate.times.front().endTime == "5:55 PM");

    LayoutFixture hidden;
    hidden.layout.sheets.front().visible = false;
    const auto hiddenResult = ScheduleWorkbookInterpreter::interpret(
        hidden.layout,
        ScheduleImportKind::Normal
        );
    assert(!hiddenResult.has_value());
    assert(hiddenResult.error().code == ErrorCode::NotFound);

    const auto cancelled = ScheduleWorkbookInterpreter::interpret(
        fixture.layout,
        ScheduleImportKind::Normal,
        [] { return true; }
        );
    assert(!cancelled.has_value());
    assert(cancelled.error().code == ErrorCode::Cancelled);
    return 0;
}
