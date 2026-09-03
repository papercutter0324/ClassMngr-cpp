#include "classmngr/engine/calendar_event_import_service.h"
#include "classmngr/engine/calendar_event_service.h"
#include "classmngr/engine/open_database.h"

#include <chrono>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using classmngr::engine::CalendarDate;
using classmngr::engine::CalendarEvent;
using classmngr::engine::CalendarEventImportService;
using classmngr::engine::CalendarImportResult;
using classmngr::engine::CalendarImportWorkbook;

CalendarDate date(int year, unsigned month, unsigned day)
{
    return {
        std::chrono::year{year},
        std::chrono::month{month},
        std::chrono::day{day}
    };
}

bool expect(bool condition, std::string_view message)
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineCalendarEventImportServiceTests: "
              << message
              << '\n';
    return false;
}

CalendarImportWorkbook julyWorkbook()
{
    CalendarImportWorkbook workbook;
    workbook.styles = {
        {},
        {"CCCCCC", ""}
    };
    workbook.cells = {
        {1, 1, 0, "JULY", ""},
        {1, 2, 0, "2026", ""}
    };
    return workbook;
}

const CalendarEvent* eventOn(
    const CalendarImportResult& parsed,
    const CalendarDate& expectedDate
    )
{
    for (const CalendarEvent& event : parsed.events)
    {
        if (event.startDate == expectedDate)
        {
            return &event;
        }
    }
    return nullptr;
}

bool testNewSemesterLegendImport()
{
    CalendarImportWorkbook workbook = julyWorkbook();
    workbook.cells.push_back({20, 26, 1, "New Semester", ""});
    workbook.cells.push_back({4, 1, 1, "6", ""});

    const CalendarImportResult parsed =
        CalendarEventImportService::parse(workbook);
    const CalendarEvent* event = eventOn(parsed, date(2026, 7, 6));

    return expect(parsed.skippedCount == 0, "new semester was skipped")
        && expect(parsed.events.size() == 1, "new semester event count changed")
        && expect(event != nullptr, "new semester date changed")
        && expect(event->title == "New Semester", "new semester title changed")
        && expect(event->eventType == "Other", "new semester event type changed")
        && expect(!event->allDay, "new semester all-day state changed")
        && expect(event->timeStatus == "Unknown", "new semester time status changed");
}

bool testWeekendAndColorOverrides()
{
    CalendarImportWorkbook weekendOnly = julyWorkbook();
    weekendOnly.cells.push_back({20, 26, 1, "Weekend", ""});
    weekendOnly.cells.push_back({3, 6, 1, "4", ""});

    const CalendarImportResult ignored =
        CalendarEventImportService::parse(weekendOnly);
    if (!expect(ignored.events.empty(), "weekend legend emitted an event")
        || !expect(ignored.skippedCount == 1, "weekend skip count changed"))
    {
        return false;
    }

    CalendarImportWorkbook workbook = julyWorkbook();
    workbook.styles.push_back({"CCCCCC", "FF0000"});
    workbook.styles.push_back({"", "FF0000"});
    workbook.cells.push_back({20, 26, 1, "Weekend", ""});
    workbook.cells.push_back({21, 26, 2, "Red Day", ""});
    workbook.cells.push_back({22, 26, 3, "Red Day", ""});
    workbook.cells.push_back({3, 6, 2, "4", ""});
    workbook.cells.push_back({4, 1, 3, "6", ""});

    const CalendarImportResult parsed =
        CalendarEventImportService::parse(workbook);
    const CalendarEvent* weekendEvent = eventOn(parsed, date(2026, 7, 4));
    const CalendarEvent* weekdayEvent = eventOn(parsed, date(2026, 7, 6));

    return expect(parsed.events.size() == 2, "colored override event count changed")
        && expect(weekendEvent != nullptr, "weekend color override was ignored")
        && expect(weekdayEvent != nullptr, "weekday font override was ignored")
        && expect(weekendEvent->title == "Red Day", "weekend override title changed")
        && expect(weekdayEvent->title == "Red Day", "weekday override title changed")
        && expect(weekendEvent->eventType == "Holiday", "weekend override type changed")
        && expect(weekdayEvent->eventType == "Holiday", "weekday override type changed");
}

bool testShiftedFirstCalendarRow()
{
    CalendarImportWorkbook workbook;
    workbook.styles = {
        {},
        {"FFF2CC", ""}
    };
    workbook.cells = {
        {1, 1, 0, "JUNE", ""},
        {1, 2, 0, "2026", ""},
        {20, 26, 1, "DYB Workshop", ""},
        {4, 1, 1, "1", ""}
    };

    const CalendarImportResult parsed =
        CalendarEventImportService::parse(workbook);
    const CalendarEvent* event = eventOn(parsed, date(2026, 6, 1));

    return expect(parsed.events.size() == 1, "shifted-row event count changed")
        && expect(event != nullptr, "shifted-row date changed")
        && expect(event->title == "DYB Workshop", "shifted-row title changed")
        && expect(event->eventType == "Workshop", "shifted-row type changed");
}

bool testFractionalDayIsIgnored()
{
    CalendarImportWorkbook workbook = julyWorkbook();
    workbook.styles[1] = {"FFF2CC", ""};
    workbook.cells.push_back({20, 26, 1, "DYB Workshop", ""});
    workbook.cells.push_back({4, 1, 1, "6.5", ""});

    const CalendarImportResult parsed =
        CalendarEventImportService::parse(workbook);

    return expect(
        parsed.events.empty(),
        "fractional calendar day was truncated into an event"
        );
}

bool testNoteRangesAndCancellation()
{
    CalendarImportWorkbook workbook = julyWorkbook();
    workbook.styles[1] = {"FFF2CC", ""};
    workbook.cells.push_back({20, 26, 1, "DYB Workshop", ""});
    workbook.cells.push_back({9, 1, 0,
                              "6-8 - Orientation\n"
                              "9/11 - Split Session\n"
                              "10 - CANCELLED", ""});
    workbook.cells.push_back({4, 1, 1, "6", ""});
    workbook.cells.push_back({4, 2, 1, "7", ""});
    workbook.cells.push_back({4, 3, 1, "8", ""});
    workbook.cells.push_back({4, 4, 1, "9", ""});
    workbook.cells.push_back({4, 5, 1, "10", ""});
    workbook.cells.push_back({4, 6, 1, "11", ""});

    const CalendarImportResult parsed =
        CalendarEventImportService::parse(workbook);
    const CalendarEvent* day6 = eventOn(parsed, date(2026, 7, 6));
    const CalendarEvent* day7 = eventOn(parsed, date(2026, 7, 7));
    const CalendarEvent* day8 = eventOn(parsed, date(2026, 7, 8));
    const CalendarEvent* day9 = eventOn(parsed, date(2026, 7, 9));
    const CalendarEvent* day10 = eventOn(parsed, date(2026, 7, 10));
    const CalendarEvent* day11 = eventOn(parsed, date(2026, 7, 11));

    return expect(parsed.events.size() == 5, "note range event count changed")
        && expect(day6 && day6->title == "Orientation", "range start changed")
        && expect(day7 && day7->title == "Orientation", "range middle changed")
        && expect(day8 && day8->title == "Orientation", "range end changed")
        && expect(day9 && day9->title == "Split Session", "two-endpoint start changed")
        && expect(day10 == nullptr, "cancelled date emitted an event")
        && expect(day11 && day11->title == "Split Session", "two-endpoint end changed");
}

bool testCampusNoteSuffixAndBoundaries()
{
    CalendarImportWorkbook workbook = julyWorkbook();
    workbook.styles[1] = {"FFF2CC", ""};
    workbook.cells.push_back({20, 26, 1, "DYB Workshop", ""});
    workbook.cells.push_back({4, 1, 1, "6", "Campus: BDG and S2"});
    workbook.cells.push_back({4, 2, 1, "7", "Campus: BDGX and S20"});

    const CalendarImportResult parsed =
        CalendarEventImportService::parse(workbook, {"BDG", "S2"});
    const CalendarEvent* matching = eventOn(parsed, date(2026, 7, 6));
    const CalendarEvent* boundaryOnly = eventOn(parsed, date(2026, 7, 7));

    return expect(parsed.events.size() == 2, "campus event count changed")
        && expect(
            matching && matching->title == "DYB Workshop (BDG, S2)",
            "campus note suffix changed"
            )
        && expect(
            boundaryOnly && boundaryOnly->title == "DYB Workshop",
            "campus token boundary changed"
            );
}

bool testDuplicateSuppressionAndSignature()
{
    CalendarImportWorkbook workbook = julyWorkbook();
    workbook.styles[1] = {"FFF2CC", ""};
    workbook.cells.push_back({20, 26, 1, "DYB Workshop", ""});
    workbook.cells.push_back({4, 1, 1, "6", ""});
    workbook.cells.push_back({30, 1, 0, "JULY", ""});
    workbook.cells.push_back({30, 2, 0, "2026", ""});
    workbook.cells.push_back({33, 1, 1, "6", ""});

    const CalendarImportResult parsed =
        CalendarEventImportService::parse(workbook);
    if (!expect(parsed.events.size() == 1, "duplicate event was not suppressed"))
    {
        return false;
    }

    CalendarEvent event;
    event.title = "  A   B  ";
    event.eventType = "Holiday";
    event.timeStatus = "Timed";
    event.allDay = true;
    event.startDate = date(2026, 7, 6);
    event.endDate = date(2026, 7, 6);
    const std::string signature =
        CalendarEventImportService::importSignature(event);

    return expect(
        signature == "A B|Holiday|2026-07-06|2026-07-06|1|Timed",
        "import signature fields changed"
        )
        && expect(
            signature == CalendarEventImportService::importSignature(event),
            "import signature was not stable"
            )
        && expect(
            CalendarEventImportService::importSignature(parsed.events.front())
                == "DYB Workshop|Workshop|2026-07-06|2026-07-06|0|Unknown",
            "parsed event signature changed"
        );
}

bool testDatabaseImportPersistence()
{
    const auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
    if (!expect(opened && *opened != nullptr, "calendar import database did not open"))
    {
        return false;
    }

    classmngr::engine::CalendarEventService service(**opened);
    if (!expect(
            !service.importParsed({{}, -1}),
            "calendar import accepted a negative parser skipped count"
            ))
    {
        return false;
    }

    CalendarImportResult parsed;
    CalendarEvent first;
    first.title = "Workshop";
    first.eventType = "Workshop";
    first.timeStatus = "Unknown";
    first.startDate = date(2026, 7, 6);
    first.endDate = date(2026, 7, 6);
    parsed.events = {first, first};
    parsed.skippedCount = 2;

    const auto firstImport = service.importParsed(parsed);
    if (!expect(
            firstImport
                && firstImport->importedCount == 1
                && firstImport->skippedCount == 3,
            "calendar import counts did not include an in-import duplicate"
            ))
    {
        return false;
    }

    const auto repeatedImport = service.importParsed(parsed);
    if (!expect(
            repeatedImport
                && repeatedImport->importedCount == 0
                && repeatedImport->skippedCount == 4,
            "calendar import did not suppress persisted duplicates"
            ))
    {
        return false;
    }

    if (!expect(
            (**opened).execute(
                "CREATE TRIGGER reject_calendar_import "
                "BEFORE INSERT ON calendar_events "
                "WHEN NEW.title = 'Reject' BEGIN "
                "SELECT RAISE(ABORT, 'injected calendar import failure'); END"
                ).has_value(),
            "calendar import failure trigger could not be created"
            ))
    {
        return false;
    }

    CalendarEvent accepted = first;
    accepted.title = "Accepted before reject";
    accepted.startDate = date(2026, 7, 7);
    accepted.endDate = date(2026, 7, 7);
    CalendarEvent rejected = accepted;
    rejected.title = "Reject";
    rejected.startDate = date(2026, 7, 8);
    rejected.endDate = date(2026, 7, 8);
    const auto failedImport = service.importParsed({{accepted, rejected}, 0});
    const auto persisted = service.loadInRange(
        date(2026, 7, 6),
        date(2026, 7, 8)
        );

    return expect(!failedImport, "calendar import did not report a write failure")
        && expect(
            persisted && persisted->size() == 1
                && persisted->front().title == "Workshop",
            "calendar import write failure was not rolled back"
            );
}
} // namespace

int main()
{
    bool passed = true;
    passed &= testNewSemesterLegendImport();
    passed &= testWeekendAndColorOverrides();
    passed &= testShiftedFirstCalendarRow();
    passed &= testFractionalDayIsIgnored();
    passed &= testNoteRangesAndCancellation();
    passed &= testCampusNoteSuffixAndBoundaries();
    passed &= testDuplicateSuppressionAndSignature();
    passed &= testDatabaseImportPersistence();
    return passed ? 0 : 1;
}
