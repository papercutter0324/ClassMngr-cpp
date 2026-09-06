#include "classmngr/engine/roster_report.h"

#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

    std::cerr << "ClassMngrEngineRosterReportServiceTests: "
              << message
              << '\n';
    return false;
}

RosterReportClass sampleClass(
    int classId = 1,
    std::string day = "Monday",
    std::string startTime = "4:00 PM",
    std::string grade = "E4",
    std::string level = "Hercules"
    )
{
    RosterReportClass result;
    result.classId = classId;
    result.classroomName = "Class " + std::to_string(classId);
    result.classGrade = std::move(grade);
    result.classLevel = std::move(level);
    result.teacherEn = "Emma";
    result.teacherKr = "엠마";
    result.roomNumber = "506";
    result.wifiName = "DYB";
    result.wifiPassword = "wifi-pw";
    result.zoomId = "zoom";
    result.zoomPassword = "zoom-pw";
    result.classTimes = {{
        std::move(day),
        std::move(startTime),
        "4:50 PM"
    }};
    result.rosterColumns = {"English", "Korean"};
    result.rosterRows = {
        {"Lily", "Lily KR"},
        {"Jay", "Jay KR"}
    };
    return result;
}

bool hasCell(
    const std::vector<RosterReportCellValue>& values,
    std::string_view page,
    int row,
    int column,
    std::string_view expected
    )
{
    for (const RosterReportCellValue& value : values)
    {
        if (value.page == page
            && value.row == row
            && value.column == column
            && value.value == expected)
        {
            return true;
        }
    }

    return false;
}
} // namespace

int main()
{
    bool passed = true;

    passed &= expect(
        RosterReportService::dailyTimeLabel("4:00 PM") == "4 p.m."
            && RosterReportService::dailyTimeLabel("16:50") == "4:50 p.m."
            && RosterReportService::dailyTimeLabel("not-a-time")
                == "not-a-time",
        "daily time labels changed"
        );

    const auto byDay = RosterReportService::buildByDayCellValues({
        sampleClass()
    });
    passed &= expect(
        byDay.has_value()
            && hasCell(
                *byDay,
                "Monday",
                3,
                2,
                "E4 Hercules"
                )
            && hasCell(
                *byDay,
                "Monday",
                4,
                2,
                "Emma (506)"
                )
            && hasCell(
                *byDay,
                "Monday",
                RosterReportService::ByDayFirstStudentRow,
                2,
                "Lily"
                )
            && hasCell(*byDay, "Monday", 30, 2, "DYB"),
        "by-day cell values changed"
        );

    RosterReportClass manyStudents = sampleClass();
    manyStudents.rosterRows.clear();
    for (int index = 1; index <= 26; ++index)
    {
        manyStudents.rosterRows.push_back({
            "Student " + std::to_string(index),
            "Korean " + std::to_string(index)
        });
    }
    const auto studentValues =
        RosterReportService::buildByDayCellValues({manyStudents});
    passed &= expect(
        studentValues.has_value()
            && hasCell(
                *studentValues,
                "Monday",
                RosterReportService::ByDayLastStudentRow,
                2,
                "Student 25"
                )
            && !hasCell(
                *studentValues,
                "Monday",
                RosterReportService::ByDayLastStudentRow + 1,
                2,
                "Student 26"
                ),
        "by-day student limit changed"
        );

    const auto duplicate = RosterReportService::buildByDayCellValues({
        sampleClass(1, "Monday", "4:00 PM"),
        sampleClass(2, "Monday", "4:30 PM")
    });
    passed &= expect(
        !duplicate.has_value()
            && duplicate.error().code == ErrorCode::InvalidArgument
            && duplicate.error().message.find(
                   "Multiple selected classes"
                   ) != std::string::npos,
        "duplicate by-day slots were not rejected"
        );

    RosterReportClass earlier = sampleClass(
        2,
        "Tuesday",
        "4:00 PM",
        "M2",
        "Tigris"
        );
    RosterReportClass later = sampleClass(
        1,
        "Tuesday",
        "6:00 PM",
        "E6",
        "Poseidon"
        );
    RosterReportClass sameTime = sampleClass(
        3,
        "Tuesday",
        "4:00 PM",
        "E5",
        "Athena"
        );
    sameTime.rosterRows = {{"", "Minseo KR"}};
    const auto daily = RosterReportService::buildDailyCellValues({
        later,
        earlier,
        sameTime
    });
    passed &= expect(
        daily.has_value()
            && hasCell(
                *daily,
                "Tuesday",
                RosterReportService::DailyFirstSectionRow,
                1,
                "E5 Athena (4 p.m. / Emma / Room 506 / Zoom: zoom \xE2\x80\xA2 PW zoom-pw)"
                )
            && hasCell(
                *daily,
                "Tuesday",
                RosterReportService::DailyFirstSectionRow
                    + RosterReportService::DailyRowsPerSection,
                1,
                "M2 Tigris (4 p.m. / Emma / Room 506 / Zoom: zoom \xE2\x80\xA2 PW zoom-pw)"
                )
            && hasCell(
                *daily,
                "Tuesday",
                RosterReportService::DailyFirstSectionRow + 1,
                RosterReportService::DailyFirstStudentColumn,
                "Minseo KR"
                ),
        "daily roster sections changed"
        );

    RosterReportClass first = sampleClass();
    first.rosterColumns = {
        "English",
        "Korean",
        "Birthday",
        "Fall",
        "Phone"
    };
    RosterReportClass second = sampleClass(2, "Tuesday", "5:00 PM");
    second.rosterColumns = {
        "English",
        "Korean",
        "Phone",
        "School",
        "Speech Contest"
    };
    const auto available =
        RosterReportService::availablePerClassExtraInfoColumns({first, second});
    passed &= expect(
        available == std::vector<std::string>{"Birthday", "Phone", "School"},
        "available per-class columns changed"
        );

    first.rosterRows = {{"Kaelyn", "Kaelyn KR", "10/15"}};
    const auto perClass =
        RosterReportService::buildPerClassExtraInfoCellValues(
            {first},
            {"Birthday", "School", "Fall"},
            RosterReportOrientation::Portrait
            );
    passed &= expect(
        perClass.has_value()
            && hasCell(*perClass, "class|0|1", 5, 1, "No.")
            && hasCell(*perClass, "class|0|1", 5, 4, "Birthday")
            && hasCell(*perClass, "class|0|1", 5, 5, "School")
            && !hasCell(*perClass, "class|0|1", 5, 6, "Fall")
            && hasCell(*perClass, "class|0|1", 6, 1, "1")
            && hasCell(*perClass, "class|0|1", 6, 4, "10/15")
            && !hasCell(*perClass, "class|0|1", 6, 5, "10/15"),
        "per-class report values changed"
        );

    passed &= expect(
        RosterReportService::perClassExtraInfoMaxColumns(
            RosterReportOrientation::Portrait
            ) == 4
            && RosterReportService::perClassExtraInfoMaxColumns(
                RosterReportOrientation::Landscape
                ) == 8,
        "per-class orientation limits changed"
        );

    return passed ? 0 : 1;
}
