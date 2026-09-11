#include "classmngr/engine/calendar_event_import_service.h"

#include "classmngr/engine/calendar_event_rules.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace classmngr::engine
{
namespace
{
constexpr int LegendColumn = 26;
constexpr int CalendarGridRows = 6;
constexpr int CalendarGridColumns = 7;
constexpr int NotesSearchRows = 8;

using CellMap = std::map<int, CalendarImportCell>;
using DateTitles = std::map<CalendarDate, std::vector<std::string>>;
using DateSet = std::set<CalendarDate>;
using StringMap = std::map<std::string, std::string>;

struct MonthBlock
{
    int year = 0;
    int month = 0;
    int row = 0;
    int column = 0;
};

char upperAscii(char value)
{
    return static_cast<char>(
        std::toupper(static_cast<unsigned char>(value))
        );
}

char lowerAscii(char value)
{
    return static_cast<char>(
        std::tolower(static_cast<unsigned char>(value))
        );
}

bool asciiWhitespace(char value)
{
    return std::isspace(static_cast<unsigned char>(value)) != 0;
}

std::string trimAsciiWhitespace(std::string_view value)
{
    std::size_t first = 0;
    while (first < value.size() && asciiWhitespace(value[first]))
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first && asciiWhitespace(value[last - 1]))
    {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

std::string simplifyAsciiWhitespace(std::string_view value)
{
    std::string result;
    bool pendingSpace = false;

    for (const char character : value)
    {
        if (asciiWhitespace(character))
        {
            if (!result.empty())
            {
                pendingSpace = true;
            }
            continue;
        }

        if (pendingSpace)
        {
            result.push_back(' ');
            pendingSpace = false;
        }

        result.push_back(character);
    }

    return result;
}

std::string lowerAscii(std::string_view value)
{
    std::string result(value);
    for (char& character : result)
    {
        character = lowerAscii(character);
    }
    return result;
}

std::string normalizedColor(std::string_view color)
{
    std::string normalized = trimAsciiWhitespace(color);
    for (char& character : normalized)
    {
        character = upperAscii(character);
    }

    if (!normalized.empty() && normalized.front() == '#')
    {
        normalized.erase(normalized.begin());
    }

    if (
        normalized.size() == 8
        && normalized.compare(0, 2, "FF") == 0
        )
    {
        normalized.erase(0, 2);
    }

    return normalized;
}

bool isMeaningfulFill(std::string_view color)
{
    const std::string normalized = normalizedColor(color);
    return !normalized.empty() && normalized != "FFFFFF";
}

bool isMeaningfulFontColor(std::string_view color)
{
    const std::string normalized = normalizedColor(color);
    return !normalized.empty()
        && normalized != "000000"
        && normalized != "FFFFFF";
}

std::string comparableFontColor(std::string_view color)
{
    const std::string normalized = normalizedColor(color);
    return normalized.empty() ? "000000" : normalized;
}

CalendarImportCell cellAt(
    const CellMap& cellsByPosition,
    int row,
    int column
    )
{
    const auto found = cellsByPosition.find(row * 100 + column);
    return found == cellsByPosition.end()
        ? CalendarImportCell{}
        : found->second;
}

std::string cellText(
    const CellMap& cellsByPosition,
    int row,
    int column
    )
{
    return trimAsciiWhitespace(
        cellAt(cellsByPosition, row, column).value
        );
}

CalendarImportStyle cellStyle(
    const CalendarImportWorkbook& workbook,
    const CalendarImportCell& cell
    )
{
    if (
        cell.style < 0
        || cell.style >= static_cast<int>(workbook.styles.size())
        )
    {
        return {};
    }

    return workbook.styles[static_cast<std::size_t>(cell.style)];
}

int currentYear(const Clock& clock)
{
    using namespace std::chrono;

    const year_month_day today{
        floor<days>(clock.nowUtc())
    };
    return static_cast<int>(today.year());
}

bool asciiDigit(char value)
{
    return value >= '0' && value <= '9';
}

int calendarYear(
    const CalendarImportWorkbook& workbook,
    const Clock& clock
    )
{
    for (const CalendarImportCell& cell : workbook.cells)
    {
        for (std::size_t index = 0; index + 3 < cell.value.size(); ++index)
        {
            if (
                cell.value[index] == '2'
                && cell.value[index + 1] == '0'
                && asciiDigit(cell.value[index + 2])
                && asciiDigit(cell.value[index + 3])
                )
            {
                return (cell.value[index + 2] - '0') * 10
                    + (cell.value[index + 3] - '0')
                    + 2000;
            }
        }
    }

    return currentYear(clock);
}

int monthNumber(std::string_view value)
{
    constexpr std::array<std::string_view, 12> months{
        "JANUARY",
        "FEBRUARY",
        "MARCH",
        "APRIL",
        "MAY",
        "JUNE",
        "JULY",
        "AUGUST",
        "SEPTEMBER",
        "OCTOBER",
        "NOVEMBER",
        "DECEMBER"
    };

    std::string normalized = trimAsciiWhitespace(value);
    for (char& character : normalized)
    {
        character = upperAscii(character);
    }

    int index = -1;
    for (int month = 0; month < static_cast<int>(months.size()); ++month)
    {
        if (months[static_cast<std::size_t>(month)] == normalized)
        {
            index = month;
            break;
        }
    }

    if (index < 0 && normalized.size() >= 3)
    {
        for (int month = 0; month < static_cast<int>(months.size()); ++month)
        {
            if (
                months[static_cast<std::size_t>(month)].compare(
                    0,
                    3,
                    normalized,
                    0,
                    3
                    ) == 0
                )
            {
                index = month;
                break;
            }
        }
    }

    return index >= 0 ? index + 1 : 0;
}

std::vector<MonthBlock> monthBlocks(
    const CalendarImportWorkbook& workbook,
    const Clock& clock
    )
{
    std::vector<MonthBlock> blocks;
    const int year = calendarYear(workbook, clock);

    for (const CalendarImportCell& cell : workbook.cells)
    {
        const int month = monthNumber(cell.value);
        if (month > 0)
        {
            blocks.push_back({year, month, cell.row, cell.column});
        }
    }

    std::sort(
        blocks.begin(),
        blocks.end(),
        [](const MonthBlock& lhs, const MonthBlock& rhs)
        {
            return lhs.month < rhs.month;
        }
        );

    return blocks;
}

CalendarDate gridDate(
    const MonthBlock& block,
    int gridStartRow,
    int row,
    int column
    )
{
    using namespace std::chrono;

    const CalendarDate firstOfMonth{
        year{block.year},
        month{static_cast<unsigned>(block.month)},
        day{1}
    };

    if (!firstOfMonth.ok())
    {
        return {};
    }

    const sys_days firstGridDate =
        sys_days{firstOfMonth}
        - days{
            static_cast<int>(
                weekday{sys_days{firstOfMonth}}.iso_encoding()
                ) - 1
        };

    return CalendarDate{
        firstGridDate
        + days{(row - gridStartRow) * 7 + column - block.column}
    };
}

std::optional<double> cellNumber(std::string_view value)
{
    const std::string source(value);
    char* end = nullptr;
    const double number = std::strtod(source.c_str(), &end);

    if (end == source.c_str())
    {
        return std::nullopt;
    }

    while (*end != '\0' && asciiWhitespace(*end))
    {
        ++end;
    }

    if (*end != '\0')
    {
        return std::nullopt;
    }

    return number;
}

bool hasDisplayedDayInRow(
    const CellMap& cellsByPosition,
    const MonthBlock& block,
    int row
    )
{
    for (
        int column = block.column;
        column < block.column + CalendarGridColumns;
        ++column
        )
    {
        const std::optional<double> displayedDay =
            cellNumber(cellAt(cellsByPosition, row, column).value);

        if (displayedDay && *displayedDay > 0)
        {
            return true;
        }
    }

    return false;
}

bool cellDisplaysDay(
    const CellMap& cellsByPosition,
    int row,
    int column,
    int day
    )
{
    const std::optional<double> displayedDay =
        cellNumber(cellAt(cellsByPosition, row, column).value);

    return displayedDay && *displayedDay == static_cast<double>(day);
}

int gridStartRow(
    const CellMap& cellsByPosition,
    const MonthBlock& block
    )
{
    using namespace std::chrono;

    const int assumedStartRow = block.row + 2;
    const CalendarDate firstOfMonth{
        year{block.year},
        month{static_cast<unsigned>(block.month)},
        day{1}
    };

    if (!firstOfMonth.ok())
    {
        return assumedStartRow;
    }

    const int firstDayColumn =
        block.column
        + static_cast<int>(
            weekday{sys_days{firstOfMonth}}.iso_encoding()
            ) - 1;

    if (
        !hasDisplayedDayInRow(cellsByPosition, block, assumedStartRow)
        && cellDisplaysDay(
            cellsByPosition,
            assumedStartRow + 1,
            firstDayColumn,
            1
            )
        )
    {
        return assumedStartRow + 1;
    }

    return assumedStartRow;
}

std::string cleanedLegendLabel(std::string label)
{
    const std::size_t parenthesisIndex = label.find('(');
    if (parenthesisIndex != std::string::npos)
    {
        label.erase(parenthesisIndex);
    }

    return simplifyAsciiWhitespace(label);
}

bool isWeekendLegend(std::string_view label)
{
    return lowerAscii(cleanedLegendLabel(std::string(label))) == "weekend";
}

bool shouldIgnoreLegend(std::string_view label)
{
    const std::string normalized =
        lowerAscii(cleanedLegendLabel(std::string(label)));

    return normalized == "weekend"
        || normalized == "creo fixed days"
        || normalized == "creo workshop"
        || normalized == "date confirmed"
        || normalized == "date not confirmed";
}

std::string eventTypeForLegend(std::string_view label)
{
    const std::string normalized =
        lowerAscii(cleanedLegendLabel(std::string(label)));

    if (
        normalized == "red day"
        || normalized == "substitute / special red day"
        || normalized == "dyb fixed days"
        )
    {
        return "Holiday";
    }

    if (normalized == "dyb workshop")
    {
        return "Workshop";
    }

    if (
        normalized.starts_with("cms")
        || normalized.find("parent meetings") != std::string::npos
        )
    {
        return "CM";
    }

    return "Other";
}

StringMap fillLegend(const CalendarImportWorkbook& workbook)
{
    StringMap legend;

    for (const CalendarImportCell& cell : workbook.cells)
    {
        if (cell.column != LegendColumn)
        {
            continue;
        }

        const CalendarImportStyle style = cellStyle(workbook, cell);
        if (!isMeaningfulFill(style.fillColor))
        {
            continue;
        }

        legend[normalizedColor(style.fillColor)] =
            simplifyAsciiWhitespace(cell.value);
    }

    return legend;
}

StringMap fontLegend(const CalendarImportWorkbook& workbook)
{
    StringMap legend;

    for (const CalendarImportCell& cell : workbook.cells)
    {
        if (cell.column != LegendColumn)
        {
            continue;
        }

        const CalendarImportStyle style = cellStyle(workbook, cell);
        if (!isMeaningfulFontColor(style.fontColor))
        {
            continue;
        }

        legend[normalizedColor(style.fontColor)] =
            simplifyAsciiWhitespace(cell.value);
    }

    return legend;
}

CalendarImportStyle weekendLegendStyle(
    const CalendarImportWorkbook& workbook
    )
{
    for (const CalendarImportCell& cell : workbook.cells)
    {
        if (
            cell.column == LegendColumn
            && isWeekendLegend(cell.value)
            )
        {
            return cellStyle(workbook, cell);
        }
    }

    return {};
}

std::optional<int> integerValue(std::string_view value)
{
    const std::string normalized = trimAsciiWhitespace(value);
    if (normalized.empty())
    {
        return std::nullopt;
    }

    int result = 0;
    for (const char character : normalized)
    {
        if (!asciiDigit(character))
        {
            return std::nullopt;
        }

        const int digit = character - '0';
        if (
            result > (
                std::numeric_limits<int>::max() - digit
                ) / 10
            )
        {
            return std::nullopt;
        }

        result = result * 10 + digit;
    }

    return result;
}

std::vector<CalendarDate> noteDates(
    int yearValue,
    int baseMonth,
    std::string_view dayStartText,
    std::string_view separator,
    std::string_view dayEndText,
    std::string_view monthOverride
    )
{
    const std::optional<int> firstDay = integerValue(dayStartText);
    if (!firstDay)
    {
        return {};
    }

    int month = baseMonth;
    int noteYear = yearValue;

    if (!trimAsciiWhitespace(monthOverride).empty())
    {
        const std::string overridePrefix =
            trimAsciiWhitespace(monthOverride).substr(0, 3);
        const int overrideMonth = monthNumber(overridePrefix);

        if (overrideMonth > 0)
        {
            month = overrideMonth;
            if (baseMonth == 12 && month == 1)
            {
                ++noteYear;
            }
            else if (baseMonth == 1 && month == 12)
            {
                --noteYear;
            }
        }
    }

    std::vector<CalendarDate> dates;
    dates.push_back({
        std::chrono::year{noteYear},
        std::chrono::month{static_cast<unsigned>(month)},
        std::chrono::day{static_cast<unsigned>(*firstDay)}
    });

    if (!trimAsciiWhitespace(dayEndText).empty())
    {
        const std::optional<int> lastDay = integerValue(dayEndText);
        if (lastDay)
        {
            if (separator == "-")
            {
                for (int day = *firstDay + 1; day <= *lastDay; ++day)
                {
                    dates.push_back({
                        std::chrono::year{noteYear},
                        std::chrono::month{static_cast<unsigned>(month)},
                        std::chrono::day{static_cast<unsigned>(day)}
                    });
                }
            }
            else if (separator == "/")
            {
                dates.push_back({
                    std::chrono::year{noteYear},
                    std::chrono::month{static_cast<unsigned>(month)},
                    std::chrono::day{static_cast<unsigned>(*lastDay)}
                });
            }
        }
    }

    dates.erase(
        std::remove_if(
            dates.begin(),
            dates.end(),
            [](const CalendarDate& date)
            {
                return !date.ok();
            }
            ),
        dates.end()
        );

    return dates;
}

const std::regex& noteEntryPattern()
{
    static const std::regex pattern(
        R"(^\s*(\d{1,2})(\s*([-/])\s*(\d{1,2}))?(\s+([A-Za-z]{3,9}))?(\s*\?\?)?\s+-\s+(.+)\s*$)"
        );
    return pattern;
}

bool containsInsensitive(std::string_view text, std::string_view value)
{
    if (value.empty() || value.size() > text.size())
    {
        return false;
    }

    for (std::size_t offset = 0; offset <= text.size() - value.size(); ++offset)
    {
        bool matches = true;
        for (std::size_t index = 0; index < value.size(); ++index)
        {
            if (
                lowerAscii(text[offset + index])
                != lowerAscii(value[index])
                )
            {
                matches = false;
                break;
            }
        }

        if (matches)
        {
            return true;
        }
    }

    return false;
}

void addNoteEntries(
    const MonthBlock& block,
    std::string_view notes,
    DateTitles* titlesByDate,
    DateSet* cancelledDates
    )
{
    std::size_t lineStart = 0;
    while (lineStart <= notes.size())
    {
        const std::size_t lineEnd = notes.find('\n', lineStart);
        const std::string rawLine =
            std::string(
                notes.substr(
                    lineStart,
                    lineEnd == std::string_view::npos
                        ? std::string_view::npos
                        : lineEnd - lineStart
                    )
                );
        const std::string line = simplifyAsciiWhitespace(rawLine);

        if (!line.empty())
        {
            std::smatch match;
            if (std::regex_match(line, match, noteEntryPattern()))
            {
                const std::vector<CalendarDate> dates = noteDates(
                    block.year,
                    block.month,
                    match[1].str(),
                    match[3].str(),
                    match[4].str(),
                    match[6].str()
                    );
                const std::string title = simplifyAsciiWhitespace(match[8].str());
                const bool cancelled = containsInsensitive(title, "CANCELLED");

                for (const CalendarDate& date : dates)
                {
                    if (cancelled)
                    {
                        cancelledDates->insert(date);
                    }
                    else
                    {
                        (*titlesByDate)[date].push_back(title);
                    }
                }
            }
        }

        if (lineEnd == std::string_view::npos)
        {
            break;
        }
        lineStart = lineEnd + 1;
    }
}

DateTitles noteTitlesByDate(
    const std::vector<MonthBlock>& blocks,
    const CellMap& cellsByPosition,
    DateSet* cancelledDates
    )
{
    DateTitles titlesByDate;

    for (const MonthBlock& block : blocks)
    {
        for (
            int row = block.row + 8;
            row <= block.row + 8 + NotesSearchRows;
            ++row
            )
        {
            const std::string text = cellText(
                cellsByPosition,
                row,
                block.column
                );

            if (text.find('-') == std::string::npos)
            {
                continue;
            }

            addNoteEntries(
                block,
                text,
                &titlesByDate,
                cancelledDates
                );
            break;
        }
    }

    return titlesByDate;
}

bool containsExact(
    const std::vector<std::string>& values,
    std::string_view candidate
    )
{
    return std::find(values.begin(), values.end(), candidate) != values.end();
}

std::vector<std::string> normalizedCampusCodes(
    const std::vector<std::string>& campusCodes
    )
{
    std::vector<std::string> codes;

    for (const std::string& code : campusCodes)
    {
        const std::string normalized = trimAsciiWhitespace(code);
        if (!normalized.empty() && !containsExact(codes, normalized))
        {
            codes.push_back(normalized);
        }
    }

    return codes;
}

bool asciiAlphaNumeric(char value)
{
    return (
        value >= 'A' && value <= 'Z'
        ) || (
            value >= 'a' && value <= 'z'
            ) || (
                value >= '0' && value <= '9'
                );
}

bool containsCampusCode(
    std::string_view text,
    std::string_view code
    )
{
    const std::string normalizedCode = trimAsciiWhitespace(code);
    if (normalizedCode.empty() || normalizedCode.size() > text.size())
    {
        return false;
    }

    for (
        std::size_t offset = 0;
        offset <= text.size() - normalizedCode.size();
        ++offset
        )
    {
        bool matches = true;
        for (std::size_t index = 0; index < normalizedCode.size(); ++index)
        {
            if (
                upperAscii(text[offset + index])
                != upperAscii(normalizedCode[index])
                )
            {
                matches = false;
                break;
            }
        }

        if (!matches)
        {
            continue;
        }

        const bool leftBoundary =
            offset == 0 || !asciiAlphaNumeric(text[offset - 1]);
        const std::size_t end = offset + normalizedCode.size();
        const bool rightBoundary =
            end == text.size() || !asciiAlphaNumeric(text[end]);

        if (leftBoundary && rightBoundary)
        {
            return true;
        }
    }

    return false;
}

std::vector<std::string> campusCodesInNote(
    std::string_view note,
    const std::vector<std::string>& knownCampusCodes
    )
{
    std::vector<std::string> codes;

    for (const std::string& code : knownCampusCodes)
    {
        const std::string normalized = trimAsciiWhitespace(code);
        if (
            containsCampusCode(note, normalized)
            && !containsExact(codes, normalized)
            )
        {
            codes.push_back(normalized);
        }
    }

    return codes;
}

std::string joinCodes(const std::vector<std::string>& codes)
{
    std::string result;
    for (std::size_t index = 0; index < codes.size(); ++index)
    {
        if (index != 0)
        {
            result += ", ";
        }
        result += codes[index];
    }
    return result;
}

std::string titleWithCampusCodes(
    std::string_view title,
    std::string_view note,
    const std::vector<std::string>& knownCampusCodes
    )
{
    std::string normalizedTitle = simplifyAsciiWhitespace(title);
    std::vector<std::string> codes =
        campusCodesInNote(note, knownCampusCodes);

    codes.erase(
        std::remove_if(
            codes.begin(),
            codes.end(),
            [&normalizedTitle](const std::string& code)
            {
                return containsCampusCode(normalizedTitle, code);
            }
            ),
        codes.end()
        );

    if (codes.empty())
    {
        return normalizedTitle;
    }

    return normalizedTitle + " (" + joinCodes(codes) + ")";
}

CalendarEvent calendarEvent(
    const CalendarDate& date,
    const std::string& title,
    const std::string& eventType
    )
{
    CalendarEvent event;
    event.title = simplifyAsciiWhitespace(title);
    event.eventType = CalendarEventRules::normalizedEventType(eventType);
    event.allDay =
        event.eventType == "Holiday"
        || event.eventType == "Vacation"
        || event.eventType == "Meeting";
    event.timeStatus = event.allDay ? "Timed" : "Unknown";
    event.startDate = date;
    event.endDate = date;
    return event;
}

std::string isoDate(const CalendarDate& date)
{
    if (!date.ok())
    {
        return {};
    }

    std::ostringstream output;
    output << std::setfill('0')
           << std::setw(4) << static_cast<int>(date.year())
           << '-'
           << std::setw(2)
           << static_cast<unsigned>(date.month())
           << '-'
           << std::setw(2)
           << static_cast<unsigned>(date.day());
    return output.str();
}

} // namespace

std::string CalendarEventImportService::importSignature(
    const CalendarEvent& event
    )
{
    return simplifyAsciiWhitespace(event.title)
        + "|"
        + CalendarEventRules::normalizedEventType(event.eventType)
        + "|"
        + isoDate(event.startDate)
        + "|"
        + isoDate(event.endDate)
        + "|"
        + (event.allDay ? "1" : "0")
        + "|"
        + CalendarEventRules::normalizedTimeStatus(event.timeStatus);
}

CalendarImportResult CalendarEventImportService::parse(
    const CalendarImportWorkbook& workbook,
    const std::vector<std::string>& campusCodes
    )
{
    const SystemClock clock;
    return parse(workbook, campusCodes, clock);
}

CalendarImportResult CalendarEventImportService::parse(
    const CalendarImportWorkbook& workbook,
    const std::vector<std::string>& campusCodes,
    const Clock& clock
    )
{
    CalendarImportResult result;
    const std::vector<std::string> knownCampusCodes =
        normalizedCampusCodes(campusCodes);
    const std::vector<MonthBlock> blocks = monthBlocks(workbook, clock);

    CellMap cellsByPosition;
    for (const CalendarImportCell& cell : workbook.cells)
    {
        cellsByPosition[cell.row * 100 + cell.column] = cell;
    }

    DateSet cancelledDates;
    const DateTitles titlesByDate = noteTitlesByDate(
        blocks,
        cellsByPosition,
        &cancelledDates
        );
    const StringMap labelsByFill = fillLegend(workbook);
    const StringMap labelsByFont = fontLegend(workbook);
    const CalendarImportStyle weekendStyle = weekendLegendStyle(workbook);
    const std::string weekendFill =
        normalizedColor(weekendStyle.fillColor);
    const std::string weekendFont =
        comparableFontColor(weekendStyle.fontColor);
    std::set<std::string> emitted;

    for (const MonthBlock& block : blocks)
    {
        const int startRow = gridStartRow(cellsByPosition, block);

        for (
            int row = startRow;
            row < startRow + CalendarGridRows;
            ++row
            )
        {
            for (
                int column = block.column;
                column < block.column + CalendarGridColumns;
                ++column
                )
            {
                const CalendarImportCell cell =
                    cellAt(cellsByPosition, row, column);

                if (trimAsciiWhitespace(cell.value).empty())
                {
                    continue;
                }

                const std::optional<double> displayedDay =
                    cellNumber(cell.value);
                if (!displayedDay || *displayedDay <= 0)
                {
                    continue;
                }

                const CalendarDate date = gridDate(
                    block,
                    startRow,
                    row,
                    column
                    );

                if (
                    !date.ok()
                    || *displayedDay
                        != static_cast<double>(
                            static_cast<unsigned>(date.day())
                            )
                    || cancelledDates.contains(date)
                    )
                {
                    continue;
                }

                const CalendarImportStyle style = cellStyle(workbook, cell);
                std::vector<std::string> labels;
                const std::string fill = normalizedColor(style.fillColor);
                const bool isWeekendFill =
                    isMeaningfulFill(weekendFill)
                    && fill == weekendFill;
                const auto fillLabel = labelsByFill.find(fill);
                if (fillLabel != labelsByFill.end())
                {
                    labels.push_back(fillLabel->second);
                }

                const std::string font = normalizedColor(style.fontColor);
                const bool matchesWeekendFont =
                    comparableFontColor(font) == weekendFont;
                const bool shouldMatchFont =
                    (
                        isWeekendFill
                        && !matchesWeekendFont
                    )
                    || (
                        static_cast<unsigned>(
                            std::chrono::weekday{
                                std::chrono::sys_days{date}
                            }.iso_encoding()
                            ) >= 1
                        && static_cast<unsigned>(
                            std::chrono::weekday{
                                std::chrono::sys_days{date}
                            }.iso_encoding()
                            ) <= 5
                        && !isMeaningfulFill(fill)
                        && isMeaningfulFontColor(font)
                    );
                const auto fontLabel = labelsByFont.find(font);
                if (
                    fontLabel != labelsByFont.end()
                    && shouldMatchFont
                    )
                {
                    labels.push_back(fontLabel->second);
                }

                std::vector<std::string> uniqueLabels;
                for (const std::string& label : labels)
                {
                    if (!containsExact(uniqueLabels, label))
                    {
                        uniqueLabels.push_back(label);
                    }
                }

                for (const std::string& label : uniqueLabels)
                {
                    if (shouldIgnoreLegend(label))
                    {
                        ++result.skippedCount;
                        continue;
                    }

                    const std::string eventType = eventTypeForLegend(label);
                    std::vector<std::string> titles;
                    const auto titlesFound = titlesByDate.find(date);
                    if (titlesFound != titlesByDate.end())
                    {
                        titles = titlesFound->second;
                    }

                    if (titles.empty())
                    {
                        titles.push_back(cleanedLegendLabel(label));
                    }

                    for (const std::string& title : titles)
                    {
                        const CalendarEvent event = calendarEvent(
                            date,
                            titleWithCampusCodes(
                                title,
                                cell.note,
                                knownCampusCodes
                                ),
                            eventType
                            );
                        const std::string signature = importSignature(event);

                        if (!emitted.insert(signature).second)
                        {
                            continue;
                        }

                        result.events.push_back(event);
                    }
                }
            }
        }
    }

    return result;
}

CalendarImportResult CalendarEventImportService::parse(
    const CalendarImportWorkbook& workbook,
    const Clock& clock,
    const std::vector<std::string>& campusCodes
    )
{
    return parse(workbook, campusCodes, clock);
}

} // namespace classmngr::engine
