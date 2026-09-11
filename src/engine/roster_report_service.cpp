#include "classmngr/engine/roster_report.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace classmngr::engine
{
namespace
{
constexpr int PerClassPortraitMaxExtraColumns = 4;
constexpr int PerClassLandscapeMaxExtraColumns = 8;
constexpr int ByDayLevelRow = 3;
constexpr int ByDayTeacherRoomRow = 4;
constexpr int ByDayWifiRow = 30;
constexpr int ByDayWifiPasswordRow = 31;
constexpr int ByDayZoomRow = 32;
constexpr int ByDayZoomPasswordRow = 33;
constexpr int DailyHeaderColumn = 1;
constexpr int DailyStudentRowCount = 5;
constexpr int DailyMaxStudentsPerClass =
    RosterReportService::DailyStudentColumnCount * DailyStudentRowCount;
constexpr int DailySectionsPerPage = 6;
constexpr int PerClassIndexColumn = 1;
constexpr int PerClassEnglishColumn = 2;
constexpr int PerClassKoreanColumn = 3;

constexpr std::string_view DaySheets[] = {
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday"
};

constexpr std::string_view SlotHours[] = {
    "4",
    "5",
    "6",
    "7",
    "8",
    "9"
};

Error error(
    ErrorCode code,
    std::string message
    )
{
    return {code, std::move(message), std::nullopt};
}

std::string trimAsciiWhitespace(std::string_view value)
{
    std::size_t first = 0;
    while (first < value.size()
           && std::isspace(static_cast<unsigned char>(value[first])) != 0)
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first
           && std::isspace(static_cast<unsigned char>(value[last - 1])) != 0)
    {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

char lowerAscii(char value)
{
    if (value >= 'A' && value <= 'Z')
    {
        return static_cast<char>(value - 'A' + 'a');
    }

    return value;
}

bool equalsAsciiInsensitive(
    std::string_view left,
    std::string_view right
    )
{
    if (left.size() != right.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (lowerAscii(left[index]) != lowerAscii(right[index]))
        {
            return false;
        }
    }

    return true;
}

bool isDay(std::string_view day)
{
    const std::string trimmed = trimAsciiWhitespace(day);
    return std::any_of(
        std::begin(DaySheets),
        std::end(DaySheets),
        [&trimmed](std::string_view candidate)
        {
            return trimmed == candidate;
        }
        );
}

std::optional<int> parsedStartMinutes(std::string_view startTime)
{
    const std::string value = trimAsciiWhitespace(startTime);
    if (value.size() == 5
        && value[2] == ':'
        && std::isdigit(static_cast<unsigned char>(value[0])) != 0
        && std::isdigit(static_cast<unsigned char>(value[1])) != 0
        && std::isdigit(static_cast<unsigned char>(value[3])) != 0
        && std::isdigit(static_cast<unsigned char>(value[4])) != 0)
    {
        const int hour =
            ((value[0] - '0') * 10) + (value[1] - '0');
        const int minute =
            ((value[3] - '0') * 10) + (value[4] - '0');
        if (hour <= 23 && minute <= 59)
        {
            return (hour * 60) + minute;
        }
        return std::nullopt;
    }

    std::size_t colon = value.find(':');
    if (colon == std::string::npos || colon == 0 || colon + 3 > value.size())
    {
        return std::nullopt;
    }

    std::size_t suffixStart = colon + 3;
    if (suffixStart > value.size())
    {
        return std::nullopt;
    }

    const std::string hourText = value.substr(0, colon);
    const std::string minuteText = value.substr(colon + 1, 2);
    if (hourText.size() > 2
        || minuteText.size() != 2
        || !std::all_of(
            hourText.begin(),
            hourText.end(),
            [](char character)
            {
                return std::isdigit(
                    static_cast<unsigned char>(character)
                    ) != 0;
            }
            )
        || !std::all_of(
            minuteText.begin(),
            minuteText.end(),
            [](char character)
            {
                return std::isdigit(
                    static_cast<unsigned char>(character)
                    ) != 0;
            }
            ))
    {
        return std::nullopt;
    }

    const std::string suffix =
        trimAsciiWhitespace(value.substr(suffixStart));
    if (!equalsAsciiInsensitive(suffix, "am")
        && !equalsAsciiInsensitive(suffix, "pm"))
    {
        return std::nullopt;
    }

    const int hour =
        ((hourText[0] - '0') * (hourText.size() == 2 ? 10 : 1))
        + (hourText.size() == 2 ? hourText[1] - '0' : 0);
    const int minute =
        ((minuteText[0] - '0') * 10) + (minuteText[1] - '0');
    if (hour < 1 || hour > 12 || minute > 59)
    {
        return std::nullopt;
    }

    const int hour24 =
        (hour % 12) + (equalsAsciiInsensitive(suffix, "pm") ? 12 : 0);
    return (hour24 * 60) + minute;
}

std::string twoDigits(int value)
{
    if (value < 10)
    {
        return "0" + std::to_string(value);
    }

    return std::to_string(value);
}

std::string dailyPageKey(std::string_view day, int pageIndex)
{
    if (pageIndex <= 0)
    {
        return std::string(day);
    }

    return std::string(day) + "|" + std::to_string(pageIndex);
}

int columnForStartTime(std::string_view startTime)
{
    const std::optional<int> minutes = parsedStartMinutes(startTime);
    if (!minutes)
    {
        return -1;
    }

    int hour = *minutes / 60;
    if (hour > 12)
    {
        hour -= 12;
    }

    switch (hour)
    {
    case 4:
        return 2;
    case 5:
        return 4;
    case 6:
        return 6;
    case 7:
        return 8;
    case 8:
        return 10;
    case 9:
        return 12;
    default:
        return -1;
    }
}

std::string rosterCell(
    const std::vector<std::string>& row,
    int column
    )
{
    if (column < 0 || column >= static_cast<int>(row.size()))
    {
        return {};
    }

    return trimAsciiWhitespace(row[static_cast<std::size_t>(column)]);
}

int rosterColumnIndex(
    const RosterReportClass& data,
    std::string_view name
    )
{
    for (std::size_t index = 0; index < data.rosterColumns.size(); ++index)
    {
        if (equalsAsciiInsensitive(data.rosterColumns[index], name))
        {
            return static_cast<int>(index);
        }
    }

    return -1;
}

std::string classLabel(const RosterReportClass& data)
{
    const std::string grade = trimAsciiWhitespace(data.classGrade);
    const std::string level = trimAsciiWhitespace(data.classLevel);

    std::string result;
    if (!grade.empty())
    {
        result = grade;
    }
    if (!level.empty())
    {
        if (!result.empty())
        {
            result += ' ';
        }
        result += level;
    }

    return result.empty()
        ? trimAsciiWhitespace(data.classroomName)
        : result;
}

std::string teacherRoomLabel(const RosterReportClass& data)
{
    std::string teacher = trimAsciiWhitespace(data.teacherEn);
    if (teacher.empty())
    {
        teacher = trimAsciiWhitespace(data.teacherKr);
    }

    const std::string room = trimAsciiWhitespace(data.roomNumber);
    if (teacher.empty())
    {
        return room.empty() ? std::string() : "(" + room + ")";
    }

    return room.empty() ? teacher : teacher + " (" + room + ")";
}

std::string teacherLabel(const RosterReportClass& data)
{
    const std::string teacher = trimAsciiWhitespace(data.teacherEn);
    return teacher.empty()
        ? trimAsciiWhitespace(data.teacherKr)
        : teacher;
}

std::string fallbackText(
    std::string_view fallback,
    std::string_view value
    )
{
    const std::string trimmed = trimAsciiWhitespace(value);
    return trimmed.empty() ? std::string(fallback) : trimmed;
}

std::string dailyClassHeader(
    const RosterReportClass& data,
    const ClassTime& time
    )
{
    const std::string teacher = fallbackText("N/A", teacherLabel(data));
    const std::string room =
        fallbackText("N/A", data.roomNumber);
    const std::string zoom = fallbackText("N/A", data.zoomId);

    std::string zoomLabel = "Zoom: " + zoom;
    const std::string password = trimAsciiWhitespace(data.zoomPassword);
    if (!password.empty())
    {
        zoomLabel += " \xE2\x80\xA2 PW " + password;
    }

    return classLabel(data)
        + " ("
        + RosterReportService::dailyTimeLabel(time.startTime)
        + " / "
        + teacher
        + " / Room "
        + room
        + " / "
        + zoomLabel
        + ")";
}

std::string dailyStudentName(
    const std::vector<std::string>& row,
    int englishColumn,
    int koreanColumn
    )
{
    const std::string english = rosterCell(row, englishColumn);
    return english.empty() ? rosterCell(row, koreanColumn) : english;
}

bool containsColumnName(
    const std::vector<std::string>& columns,
    std::string_view name
    )
{
    return std::any_of(
        columns.begin(),
        columns.end(),
        [name](const std::string& column)
        {
            return equalsAsciiInsensitive(column, name);
        }
        );
}

bool isPerClassExtraInfoColumn(std::string_view name)
{
    const std::string trimmed = trimAsciiWhitespace(name);
    if (trimmed.empty())
    {
        return false;
    }

    for (const std::string_view excluded : {
             std::string_view("English"),
             std::string_view("Korean"),
             std::string_view("Winter"),
             std::string_view("Speech Contest"),
             std::string_view("Summer"),
             std::string_view("Fall"),
             std::string_view("Autumn")
         })
    {
        if (equalsAsciiInsensitive(trimmed, excluded))
        {
            return false;
        }
    }

    return true;
}

std::vector<std::string> limitedPerClassExtraColumns(
    const std::vector<std::string>& selectedExtraColumns,
    RosterReportOrientation orientation
    )
{
    std::vector<std::string> columns;
    const int maxColumns =
        RosterReportService::perClassExtraInfoMaxColumns(orientation);

    for (const std::string& column : selectedExtraColumns)
    {
        const std::string trimmed = trimAsciiWhitespace(column);
        if (!isPerClassExtraInfoColumn(trimmed)
            || containsColumnName(columns, trimmed))
        {
            continue;
        }

        columns.push_back(trimmed);
        if (static_cast<int>(columns.size()) >= maxColumns)
        {
            break;
        }
    }

    return columns;
}

std::vector<std::string> perClassTimeLabels(
    const std::vector<ClassTime>& classTimes
    )
{
    std::vector<std::string> labels;
    for (const ClassTime& time : classTimes)
    {
        const std::string day = trimAsciiWhitespace(time.day);
        const std::string timeLabel =
            RosterReportService::dailyTimeLabel(time.startTime);

        if (day.empty() && timeLabel.empty())
        {
            continue;
        }

        if (day.empty())
        {
            labels.push_back(timeLabel);
        }
        else if (timeLabel.empty())
        {
            labels.push_back(day);
        }
        else
        {
            labels.push_back(day.substr(0, 3) + " - " + timeLabel);
        }
    }

    return labels;
}

std::string join(
    const std::vector<std::string>& values,
    std::string_view separator
    )
{
    std::string result;
    for (const std::string& value : values)
    {
        if (!result.empty())
        {
            result += separator;
        }
        result += value;
    }
    return result;
}

void appendCellValue(
    std::vector<RosterReportCellValue>& values,
    std::string_view page,
    int row,
    int column,
    std::string value
    )
{
    value = trimAsciiWhitespace(value);
    if (value.empty())
    {
        return;
    }

    values.push_back({
        std::string(page),
        row,
        column,
        std::move(value)
    });
}

std::string perClassPageKey(int pageIndex, int classId)
{
    std::string result = "class|" + std::to_string(pageIndex);
    if (classId > 0)
    {
        result += "|" + std::to_string(classId);
    }
    return result;
}

} // namespace

int RosterReportService::perClassExtraInfoMaxColumns(
    RosterReportOrientation orientation
    )
{
    return orientation == RosterReportOrientation::Landscape
        ? PerClassLandscapeMaxExtraColumns
        : PerClassPortraitMaxExtraColumns;
}

std::vector<std::string> RosterReportService::availablePerClassExtraInfoColumns(
    const std::vector<RosterReportClass>& classes
    )
{
    std::vector<std::string> columns;
    for (const RosterReportClass& data : classes)
    {
        for (const std::string& column : data.rosterColumns)
        {
            const std::string trimmed = trimAsciiWhitespace(column);
            if (isPerClassExtraInfoColumn(trimmed)
                && !containsColumnName(columns, trimmed))
            {
                columns.push_back(trimmed);
            }
        }
    }
    return columns;
}

std::string RosterReportService::dailyTimeLabel(std::string_view startTime)
{
    const std::string trimmed = trimAsciiWhitespace(startTime);
    const std::optional<int> minutes = parsedStartMinutes(trimmed);
    if (!minutes)
    {
        return trimmed;
    }

    const int hour24 = *minutes / 60;
    const int hour = hour24 % 12 == 0 ? 12 : hour24 % 12;
    const std::string suffix = hour24 < 12 ? "a.m." : "p.m.";
    const int minute = *minutes % 60;
    if (minute == 0)
    {
        return std::to_string(hour) + " " + suffix;
    }

    return std::to_string(hour) + ":" + twoDigits(minute) + " " + suffix;
}

Result<std::vector<RosterReportCellValue>>
RosterReportService::buildByDayCellValues(
    const std::vector<RosterReportClass>& classes
    )
{
    std::vector<RosterReportCellValue> values;
    std::set<std::string> occupiedSlots;

    for (const RosterReportClass& data : classes)
    {
        const int englishColumn = rosterColumnIndex(data, "English");
        const int koreanColumn = rosterColumnIndex(data, "Korean");

        for (const ClassTime& time : data.classTimes)
        {
            const std::string day = trimAsciiWhitespace(time.day);
            if (!isDay(day))
            {
                continue;
            }

            const int column = columnForStartTime(time.startTime);
            if (column < 0)
            {
                continue;
            }

            const std::string slotKey =
                day + "|" + std::to_string(column);
            if (!occupiedSlots.insert(slotKey).second)
            {
                return std::unexpected(error(
                    ErrorCode::InvalidArgument,
                    "Multiple selected classes use the "
                        + day
                        + " "
                        + trimAsciiWhitespace(time.startTime)
                        + " slot."
                    ));
            }

            appendCellValue(
                values,
                day,
                ByDayLevelRow,
                column,
                classLabel(data)
                );
            appendCellValue(
                values,
                day,
                ByDayTeacherRoomRow,
                column,
                teacherRoomLabel(data)
                );
            appendCellValue(values, day, ByDayWifiRow, column, data.wifiName);
            appendCellValue(
                values,
                day,
                ByDayWifiPasswordRow,
                column,
                data.wifiPassword
                );
            appendCellValue(values, day, ByDayZoomRow, column, data.zoomId);
            appendCellValue(
                values,
                day,
                ByDayZoomPasswordRow,
                column,
                data.zoomPassword
                );

            int writtenStudentCount = 0;
            for (const std::vector<std::string>& row : data.rosterRows)
            {
                if (writtenStudentCount >= RosterReportService::ByDayLastStudentRow
                        - RosterReportService::ByDayFirstStudentRow
                        + 1)
                {
                    break;
                }

                const std::string english = rosterCell(row, englishColumn);
                const std::string korean = rosterCell(row, koreanColumn);
                if (english.empty() && korean.empty())
                {
                    continue;
                }

                const int outputRow =
                    RosterReportService::ByDayFirstStudentRow
                    + writtenStudentCount;
                appendCellValue(values, day, outputRow, column, english);
                appendCellValue(values, day, outputRow, column + 1, korean);
                ++writtenStudentCount;
            }
        }
    }

    return values;
}

Result<std::vector<RosterReportCellValue>>
RosterReportService::buildDailyCellValues(
    const std::vector<RosterReportClass>& classes
    )
{
    struct DailyClassSection
    {
        const RosterReportClass* data = nullptr;
        ClassTime time;
        int inputIndex = 0;
    };

    std::vector<RosterReportCellValue> values;
    for (const std::string_view day : DaySheets)
    {
        std::vector<DailyClassSection> sections;
        int inputIndex = 0;

        for (const RosterReportClass& data : classes)
        {
            for (const ClassTime& time : data.classTimes)
            {
                if (trimAsciiWhitespace(time.day) != day)
                {
                    continue;
                }

                sections.push_back({&data, time, inputIndex});
                ++inputIndex;
            }
        }

        std::stable_sort(
            sections.begin(),
            sections.end(),
            [](const DailyClassSection& left, const DailyClassSection& right)
            {
                const std::optional<int> leftTime =
                    parsedStartMinutes(left.time.startTime);
                const std::optional<int> rightTime =
                    parsedStartMinutes(right.time.startTime);

                if (leftTime.has_value() != rightTime.has_value())
                {
                    return leftTime.has_value();
                }

                if (leftTime && rightTime && *leftTime != *rightTime)
                {
                    return *leftTime < *rightTime;
                }

                const std::string leftLabel = classLabel(*left.data);
                const std::string rightLabel = classLabel(*right.data);
                if (leftLabel != rightLabel)
                {
                    return leftLabel < rightLabel;
                }

                return left.inputIndex < right.inputIndex;
            }
            );

        for (std::size_t sectionIndex = 0;
             sectionIndex < sections.size();
             ++sectionIndex)
        {
            const DailyClassSection& section = sections[sectionIndex];
            const int pageIndex =
                static_cast<int>(sectionIndex / DailySectionsPerPage);
            const int pageSectionIndex =
                static_cast<int>(sectionIndex % DailySectionsPerPage);
            const std::string page = dailyPageKey(day, pageIndex);
            const int headerRow =
                RosterReportService::DailyFirstSectionRow
                + (pageSectionIndex
                   * RosterReportService::DailyRowsPerSection);

            appendCellValue(
                values,
                page,
                headerRow,
                DailyHeaderColumn,
                dailyClassHeader(*section.data, section.time)
                );

            const int englishColumn =
                rosterColumnIndex(*section.data, "English");
            const int koreanColumn =
                rosterColumnIndex(*section.data, "Korean");

            int writtenStudentCount = 0;
            for (const std::vector<std::string>& row : section.data->rosterRows)
            {
                if (writtenStudentCount >= DailyMaxStudentsPerClass)
                {
                    break;
                }

                const std::string name =
                    dailyStudentName(row, englishColumn, koreanColumn);
                if (name.empty())
                {
                    continue;
                }

                appendCellValue(
                    values,
                    page,
                    headerRow
                        + 1
                        + (writtenStudentCount
                           / RosterReportService::DailyStudentColumnCount),
                    RosterReportService::DailyFirstStudentColumn
                        + (writtenStudentCount
                           % RosterReportService::DailyStudentColumnCount),
                    name
                    );
                ++writtenStudentCount;
            }
        }
    }

    return values;
}

Result<std::vector<RosterReportCellValue>>
RosterReportService::buildPerClassExtraInfoCellValues(
    const std::vector<RosterReportClass>& classes,
    const std::vector<std::string>& selectedExtraColumns,
    RosterReportOrientation orientation
    )
{
    const std::vector<std::string> extraColumns =
        limitedPerClassExtraColumns(selectedExtraColumns, orientation);
    std::vector<RosterReportCellValue> values;

    for (std::size_t classIndex = 0; classIndex < classes.size(); ++classIndex)
    {
        const RosterReportClass& data = classes[classIndex];
        const std::string page = perClassPageKey(
            static_cast<int>(classIndex),
            data.classId
            );

        appendCellValue(values, page, 1, 1, "Level");
        appendCellValue(values, page, 1, 2, classLabel(data));
        appendCellValue(values, page, 1, 3, "Room");
        appendCellValue(values, page, 1, 4, data.roomNumber);

        appendCellValue(values, page, 2, 1, "Days/Times");
        appendCellValue(
            values,
            page,
            2,
            2,
            join(perClassTimeLabels(data.classTimes), "; ")
            );
        appendCellValue(values, page, 2, 3, "Wifi");
        appendCellValue(values, page, 2, 4, data.wifiName);

        appendCellValue(values, page, 3, 1, "Teacher");
        appendCellValue(values, page, 3, 2, teacherLabel(data));
        appendCellValue(values, page, 3, 3, "Wifi Password");
        appendCellValue(values, page, 3, 4, data.wifiPassword);

        appendCellValue(values, page, 4, 1, "ZOOM");
        appendCellValue(values, page, 4, 2, data.zoomId);
        appendCellValue(values, page, 4, 3, "Zoom Password");
        appendCellValue(values, page, 4, 4, data.zoomPassword);

        appendCellValue(
            values,
            page,
            RosterReportService::PerClassHeaderRow,
            PerClassIndexColumn,
            "No."
            );
        appendCellValue(
            values,
            page,
            RosterReportService::PerClassHeaderRow,
            PerClassEnglishColumn,
            "English Name"
            );
        appendCellValue(
            values,
            page,
            RosterReportService::PerClassHeaderRow,
            PerClassKoreanColumn,
            "Korean Name"
            );

        for (std::size_t index = 0; index < extraColumns.size(); ++index)
        {
            appendCellValue(
                values,
                page,
                RosterReportService::PerClassHeaderRow,
                RosterReportService::PerClassFirstExtraColumn
                    + static_cast<int>(index),
                extraColumns[index]
                );
        }

        const int englishColumn = rosterColumnIndex(data, "English");
        const int koreanColumn = rosterColumnIndex(data, "Korean");
        std::vector<int> extraColumnIndexes;
        extraColumnIndexes.reserve(extraColumns.size());
        for (const std::string& extraColumn : extraColumns)
        {
            extraColumnIndexes.push_back(
                rosterColumnIndex(data, extraColumn)
                );
        }

        for (int rowIndex = 0;
             rowIndex < RosterReportService::PerClassStudentRowCount;
             ++rowIndex)
        {
            const int outputRow =
                RosterReportService::PerClassFirstStudentRow + rowIndex;
            appendCellValue(
                values,
                page,
                outputRow,
                PerClassIndexColumn,
                std::to_string(rowIndex + 1)
                );

            if (rowIndex >= static_cast<int>(data.rosterRows.size()))
            {
                continue;
            }

            const std::vector<std::string>& row =
                data.rosterRows[static_cast<std::size_t>(rowIndex)];
            appendCellValue(
                values,
                page,
                outputRow,
                PerClassEnglishColumn,
                rosterCell(row, englishColumn)
                );
            appendCellValue(
                values,
                page,
                outputRow,
                PerClassKoreanColumn,
                rosterCell(row, koreanColumn)
                );

            for (std::size_t index = 0;
                 index < extraColumnIndexes.size();
                 ++index)
            {
                appendCellValue(
                    values,
                    page,
                    outputRow,
                    RosterReportService::PerClassFirstExtraColumn
                        + static_cast<int>(index),
                    rosterCell(row, extraColumnIndexes[index])
                    );
            }
        }
    }

    return values;
}

} // namespace classmngr::engine
