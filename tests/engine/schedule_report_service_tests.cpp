#include "classmngr/engine/schedule_report.h"

#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace
{
using namespace classmngr::engine;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineScheduleReportServiceTests: "
              << message
              << '\n';
    return false;
}

ScheduleReportEntry entry(
    int classId = 1,
    std::string grade = "E5"
    )
{
    ScheduleReportEntry result;
    result.classId = classId;
    result.teacherKr = "박지혜";
    result.teacherEn = "Jihye Park";
    result.teacherPreferredName = "J. Park";
    result.roomNumber = "413";
    result.classGrade = std::move(grade);
    result.classLevel = "Zeus";
    result.classColor = "#C9D8A6";
    result.fontColor = "#000000";
    return result;
}

ScheduleReportBuildResult blankResult(
    std::vector<std::string> days,
    std::vector<std::string> rowLabels
    )
{
    ScheduleReportBuildResult result;
    result.days = std::move(days);
    for (const std::string& day : result.days)
    {
        result.schedule.emplace(day, std::map<std::string,
            std::vector<ScheduleReportEntry>>{});
    }
    for (std::string& rowLabel : rowLabels)
    {
        result.rows.push_back({std::move(rowLabel)});
    }
    return result;
}

const ScheduleReportCell* cellAt(
    const ScheduleReportModel& model,
    std::string_view day,
    std::string_view timeLabel
    )
{
    for (const ScheduleReportRowView& row : model.rows)
    {
        if (row.timeLabel != timeLabel)
        {
            continue;
        }
        for (const ScheduleReportCell& cell : row.cells)
        {
            if (cell.day == day)
            {
                return &cell;
            }
        }
    }
    return nullptr;
}
} // namespace

int main()
{
    bool passed = true;

    passed &= expect(
        ScheduleReportService::visibleDays(true)
            == std::vector<std::string>{
                "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
                "Saturday", "Sunday"
            },
        "visible days did not preserve the weekday/weekend order"
        );
    passed &= expect(
        ScheduleReportService::defaultSlotState("Monday", "15:00", false)
            == ScheduleReportService::emptySlotState()
            && ScheduleReportService::defaultSlotState(
                "Monday", "16:00", false
                ) == ScheduleReportService::essaySlotState()
            && ScheduleReportService::defaultSlotState(
                "Saturday", "16:00", false
                ) == ScheduleReportService::emptySlotState()
            && ScheduleReportService::defaultSlotState(
                "Monday", "09:00", true
                ) == ScheduleReportService::essaySlotState(),
        "regular and intensive slot defaults changed"
        );
    passed &= expect(
        ScheduleReportService::rangeLabel("16:00", false, false)
            == "4:00 -\n4:50 PM"
            && ScheduleReportService::rangeLabel("16:00", true, true)
                == "16:00 - 16:55",
        "schedule time-range formatting changed"
        );

    passed &= expect(
        ScheduleReportService::classLine(" E5 ", " Zeus ")
            == "E5 - Zeus"
            && ScheduleReportService::classLine(" E5 ", " Zeus ", true)
                == "E5-Zeus"
            && ScheduleReportService::classLine({}, " Zeus ") == "Zeus",
        "schedule class-line formatting changed"
        );

    passed &= expect(
        ScheduleReportService::excelDayLabel("Monday")
            == "\xEC\x9B\x94(MON)"
            && ScheduleReportService::excelDayLabel("Sunday")
                == "\xEC\x9D\xBC(SUN)"
            && ScheduleReportService::excelDayLabel("Holiday") == "Holiday",
        "Excel day labels changed"
        );

    passed &= expect(
        ScheduleReportService::excelTimeLabel("4:00 PM -\n4:50 PM")
            == "4:00~4:50"
            && ScheduleReportService::excelTimeLabel("16:00 - 16:55")
                == "16:00~16:55",
        "Excel time labels changed"
        );

    ScheduleReportEntry named = entry();
    passed &= expect(
        ScheduleReportService::teacherRoomLine(named, false)
            == "박지혜 413"
            && ScheduleReportService::teacherRoomLine(named, true)
                == "J. Park 413",
        "teacher/room report line did not preserve language selection"
        );

    const std::vector<std::string> days{"Monday", "Tuesday"};
    ScheduleReportBuildResult regular = blankResult(
        days,
        {"16:00", "17:00"}
        );
    regular.schedule["Monday"]["16:00"] = {entry(1), entry(2)};
    regular.schedule["Tuesday"]["17:00"] = {entry(3)};

    ScheduleReportRequest regularRequest;
    regularRequest.days = days;
    const ScheduleReportModel regularModel = ScheduleReportService::build(
        regular,
        regularRequest
        );
    const ScheduleReportCell* doubleCell = cellAt(
        regularModel,
        "Monday",
        "16:00"
        );
    passed &= expect(
        regularModel.rows.size() == 2
            && doubleCell != nullptr
            && doubleCell->entries.size() == 2
            && regularModel.rows.front().maxEntryCount == 2
            && regularModel.summary.scheduledBlocks == 4
            && regularModel.summary.essayBlocks == 2,
        "regular report rows or summary counts changed"
        );

    ScheduleReportBuildResult intensive = blankResult(
        ScheduleReportService::visibleDays(false),
        {"15:00", "16:00", "17:00", "18:00", "19:00", "20:00", "21:00"}
        );
    intensive.schedule["Monday"]["16:00"].push_back(entry());
    ScheduleReportRequest intensiveRequest;
    intensiveRequest.days = intensive.days;
    intensiveRequest.displayMode = ScheduleReportDisplayMode::Intensive;
    intensiveRequest.rowFilter = ScheduleReportRowFilter::TrimEmptyOuterRows;
    for (const ScheduleReportRow& row : intensive.rows)
    {
        for (const std::string& day : intensive.days)
        {
            intensiveRequest.slotStateOverrides.insert({
                ScheduleReportService::slotKey(day, row.label),
                ScheduleReportService::emptySlotState()
            });
        }
    }
    intensiveRequest.slotStateOverrides[
        ScheduleReportService::slotKey("Wednesday", "18:00")
        ] = ScheduleReportService::essaySlotState();
    intensiveRequest.slotStateOverrides[
        ScheduleReportService::slotKey("Friday", "20:00")
        ] = ScheduleReportService::lunchSlotState();
    const ScheduleReportModel intensiveModel = ScheduleReportService::build(
        intensive,
        intensiveRequest
        );
    passed &= expect(
        intensiveModel.rows.size() == 5
            && intensiveModel.rows.front().timeLabel == "16:00"
            && intensiveModel.rows.back().timeLabel == "20:00",
        "intensive report trimming did not retain the visible range"
        );

    ScheduleReportBuildResult testing = blankResult(
        {"Monday", "Tuesday", "Wednesday", "Thursday"},
        {"16:00"}
        );
    testing.schedule["Monday"]["16:00"] = {entry(1), entry(2, "M2")};
    testing.schedule["Tuesday"]["16:00"] = {entry(2, "M2")};
    testing.schedule["Wednesday"]["16:00"] = {entry(3, "M1")};
    testing.schedule["Thursday"]["16:00"] = {entry(4, "M3")};

    ScheduleReportRequest testingRequest;
    testingRequest.days = testing.days;
    testingRequest.displayMode = ScheduleReportDisplayMode::Testing;
    const auto plainAssignment =
        [](std::string day, std::string room)
        {
            ScheduleReportTestingAssignmentView view;
            view.assignment.day = std::move(day);
            view.assignment.startTime = "16:00";
            view.assignment.room = std::move(room);
            return view;
        };
    testingRequest.testingAssignments.emplace(
        ScheduleReportService::slotKey("Monday", "16:00"),
        plainAssignment("Monday", "Hidden Room")
        );
    testingRequest.testingAssignments.emplace(
        ScheduleReportService::slotKey("Tuesday", "16:00"),
        plainAssignment("Tuesday", "402")
        );
    testingRequest.testingAssignments.emplace(
        ScheduleReportService::slotKey("Wednesday", "16:00"),
        plainAssignment("Wednesday", "Library")
        );

    const ScheduleReportModel testingModel = ScheduleReportService::build(
        testing,
        testingRequest
        );
    const ScheduleReportCell* mondayTesting = cellAt(
        testingModel,
        "Monday",
        "16:00"
        );
    const ScheduleReportCell* thursdayTesting = cellAt(
        testingModel,
        "Thursday",
        "16:00"
        );
    passed &= expect(
        mondayTesting != nullptr
            && mondayTesting->entries.empty()
            && mondayTesting->slotState
                == ScheduleReportService::testingSlotState()
            && mondayTesting->testingRoom == "Hidden Room"
            && thursdayTesting != nullptr
            && thursdayTesting->entries.empty()
            && thursdayTesting->slotState
                == ScheduleReportService::essaySlotState()
            && thursdayTesting->testingBlockCreationEnabled
            && testingModel.summary.testingBlocks == 3,
        "testing-mode suppression and plain assignments changed"
        );

    ScheduleReportEntry special = entry(91, "M2");
    special.kind = ScheduleReportEntryKind::TestingClass;
    special.className = "Writing Lab";
    ScheduleReportTestingAssignmentView specialAssignment;
    specialAssignment.assignment.kind =
        ScheduleReportTestingAssignmentKind::SpecialClass;
    specialAssignment.assignment.classId = 91;
    specialAssignment.testingClassEntry = special;
    testingRequest.testingAssignments.clear();
    testingRequest.testingAssignments.emplace(
        ScheduleReportService::slotKey("Monday", "16:00"),
        specialAssignment
        );
    const ScheduleReportModel specialModel = ScheduleReportService::build(
        testing,
        testingRequest
        );
    const ScheduleReportCell* specialCell = cellAt(
        specialModel,
        "Monday",
        "16:00"
        );
    passed &= expect(
        specialCell != nullptr
            && specialCell->testingClassAssignment
            && specialCell->testingClassId == 91
            && specialCell->entries.size() == 1
            && specialCell->entries.front().className == "Writing Lab"
            && specialModel.summary.testingClassBlocks == 1
            && specialModel.summary.testingBlocks == 0,
        "special testing-class assignments changed the report cell"
        );

    return passed ? 0 : 1;
}
