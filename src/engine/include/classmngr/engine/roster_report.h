#pragma once

#include "classmngr/engine/class_info.h"
#include "classmngr/engine/result.h"

#include <string>
#include <string_view>
#include <vector>

namespace classmngr::engine
{

enum class RosterReportOrientation
{
    Portrait,
    Landscape
};

struct RosterReportClass
{
    int classId = -1;
    std::string classroomName;
    std::string classGrade;
    std::string classLevel;
    std::string teacherEn;
    std::string teacherKr;
    std::string roomNumber;
    std::string wifiName;
    std::string wifiPassword;
    std::string zoomId;
    std::string zoomPassword;
    std::vector<ClassTime> classTimes;
    std::vector<std::string> rosterColumns;
    std::vector<std::vector<std::string>> rosterRows;
};

struct RosterReportCellValue
{
    std::string page;
    int row = 0;
    int column = 0;
    std::string value;
};

class RosterReportService final
{
public:
    inline static constexpr int ByDayFirstStudentRow = 5;
    inline static constexpr int ByDayLastStudentRow = 29;
    inline static constexpr int DailyFirstSectionRow = 3;
    inline static constexpr int DailyRowsPerSection = 7;
    inline static constexpr int DailyFirstStudentColumn = 2;
    inline static constexpr int DailyStudentColumnCount = 5;
    inline static constexpr int PerClassHeaderRow = 5;
    inline static constexpr int PerClassFirstStudentRow = 6;
    inline static constexpr int PerClassFirstExtraColumn = 4;
    inline static constexpr int PerClassStudentRowCount = 23;

    [[nodiscard]] static int perClassExtraInfoMaxColumns(
        RosterReportOrientation orientation
        );

    [[nodiscard]] static std::vector<std::string>
    availablePerClassExtraInfoColumns(
        const std::vector<RosterReportClass>& classes
        );

    [[nodiscard]] static std::string dailyTimeLabel(
        std::string_view startTime
        );

    [[nodiscard]] static Result<std::vector<RosterReportCellValue>>
    buildByDayCellValues(
        const std::vector<RosterReportClass>& classes
        );

    [[nodiscard]] static Result<std::vector<RosterReportCellValue>>
    buildDailyCellValues(
        const std::vector<RosterReportClass>& classes
        );

    [[nodiscard]] static Result<std::vector<RosterReportCellValue>>
    buildPerClassExtraInfoCellValues(
        const std::vector<RosterReportClass>& classes,
        const std::vector<std::string>& selectedExtraColumns,
        RosterReportOrientation orientation
        );
};

} // namespace classmngr::engine
