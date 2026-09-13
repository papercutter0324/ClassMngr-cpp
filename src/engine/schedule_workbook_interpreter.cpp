#include "classmngr/engine/schedule_workbook_interpreter.h"

#include "classmngr/engine/class_info_config.h"
#include "classmngr/engine/schedule_import_rules.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace classmngr::engine
{
namespace
{
using Cell = ScheduleWorkbookLayoutCell;
using Range = ScheduleWorkbookLayoutRange;
using Sheet = ScheduleWorkbookLayoutSheet;
using Style = ScheduleWorkbookLayoutStyle;

constexpr int IntensiveFirstHour = 9;
constexpr int IntensiveFinalHour = 21;
constexpr std::int64_t PositionStride = 20'000;

struct RawTimeRange
{
    int startHour = -1;
    int startMinute = -1;
    int endHour = -1;
    int endMinute = -1;

    [[nodiscard]] bool valid() const noexcept
    {
        return startHour >= 1
            && startHour <= 12
            && startMinute >= 0
            && startMinute <= 59
            && endHour >= 1
            && endHour <= 12
            && endMinute >= 0
            && endMinute <= 59;
    }
};

struct TimetableRow
{
    int row = 0;
    RawTimeRange raw;
    int startMinutes = -1;
    int endMinutes = -1;
};

struct ResolvedTimeRange
{
    int startMinutes = -1;
    int endMinutes = -1;

    [[nodiscard]] bool valid() const noexcept
    {
        return startMinutes >= 0 && endMinutes > startMinutes;
    }
};

struct ParsedClassCell
{
    bool parsed = false;
    std::string teacherKey;
    std::string teacherKr;
    std::string room;
    std::string grade;
    std::string level;
    RawTimeRange explicitTime;
};

struct ParsedScheduleOccurrence
{
    std::string teacherKey;
    std::string teacherKr;
    std::string room;
    std::string color;
    std::string classGrade;
    std::string classLevel;
    ClassTime time;
    std::string sourceCell;
};

struct OccurrencePartition
{
    bool valid = false;
    int score = std::numeric_limits<int>::min();
    std::vector<std::vector<int>> groups;
};

using CellIndex = std::unordered_map<std::int64_t, std::size_t>;

std::int64_t positionKey(int row, int column) noexcept
{
    return static_cast<std::int64_t>(row) * PositionStride + column;
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

std::string simplifyAsciiWhitespace(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    bool pendingSpace = false;
    for (const char character : value)
    {
        if (std::isspace(static_cast<unsigned char>(character)) != 0)
        {
            pendingSpace = !result.empty();
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

std::string upperAscii(std::string value)
{
    for (char& character : value)
    {
        if (character >= 'a' && character <= 'z')
        {
            character = static_cast<char>(character - 'a' + 'A');
        }
    }
    return value;
}

std::string lowerAscii(std::string value)
{
    for (char& character : value)
    {
        if (character >= 'A' && character <= 'Z')
        {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }
    return value;
}

bool equalsInsensitive(std::string_view left, std::string_view right)
{
    return lowerAscii(trimAsciiWhitespace(left))
        == lowerAscii(trimAsciiWhitespace(right));
}

bool startsWithAt(
    std::string_view value,
    std::size_t offset,
    std::string_view prefix
    )
{
    return offset <= value.size()
        && prefix.size() <= value.size() - offset
        && value.compare(offset, prefix.size(), prefix) == 0;
}

bool isAsciiDigit(char value) noexcept
{
    return value >= '0' && value <= '9';
}

bool isAsciiLetter(char value) noexcept
{
    return (value >= 'A' && value <= 'Z')
        || (value >= 'a' && value <= 'z');
}

bool isAsciiWord(char value) noexcept
{
    return isAsciiLetter(value) || isAsciiDigit(value) || value == '_';
}

std::optional<std::pair<int, std::size_t>> decimalAt(
    std::string_view value,
    std::size_t offset,
    std::size_t minimumDigits,
    std::size_t maximumDigits
    )
{
    if (offset >= value.size() || !isAsciiDigit(value[offset]))
    {
        return std::nullopt;
    }

    std::size_t end = offset;
    while (end < value.size()
           && end - offset < maximumDigits
           && isAsciiDigit(value[end]))
    {
        ++end;
    }
    if (end - offset < minimumDigits)
    {
        return std::nullopt;
    }

    int result = 0;
    for (std::size_t index = offset; index < end; ++index)
    {
        result = result * 10 + value[index] - '0';
    }
    return std::pair<int, std::size_t>{result, end};
}

std::size_t skipAsciiWhitespace(std::string_view value, std::size_t offset)
{
    while (offset < value.size()
           && std::isspace(static_cast<unsigned char>(value[offset])) != 0)
    {
        ++offset;
    }
    return offset;
}

RawTimeRange rawTimeRange(std::string_view value)
{
    for (std::size_t offset = 0; offset < value.size(); ++offset)
    {
        const auto startHour = decimalAt(value, offset, 1, 2);
        if (!startHour)
        {
            continue;
        }

        std::size_t cursor = skipAsciiWhitespace(value, startHour->second);
        if (cursor >= value.size() || value[cursor] != ':')
        {
            continue;
        }
        cursor = skipAsciiWhitespace(value, cursor + 1);
        const auto startMinute = decimalAt(value, cursor, 2, 2);
        if (!startMinute)
        {
            continue;
        }
        cursor = skipAsciiWhitespace(value, startMinute->second);

        std::size_t delimiterLength = 0;
        if (cursor < value.size()
            && (value[cursor] == '~' || value[cursor] == '-'))
        {
            delimiterLength = 1;
        }
        else if (startsWithAt(value, cursor, "\xE2\x80\x93")
                 || startsWithAt(value, cursor, "\xE2\x80\x94"))
        {
            delimiterLength = 3;
        }
        if (delimiterLength == 0)
        {
            continue;
        }

        cursor = skipAsciiWhitespace(value, cursor + delimiterLength);
        const auto endHour = decimalAt(value, cursor, 1, 2);
        if (!endHour)
        {
            continue;
        }
        cursor = skipAsciiWhitespace(value, endHour->second);
        if (cursor >= value.size() || value[cursor] != ':')
        {
            continue;
        }
        cursor = skipAsciiWhitespace(value, cursor + 1);
        const auto endMinute = decimalAt(value, cursor, 2, 2);
        if (!endMinute)
        {
            continue;
        }

        RawTimeRange result{
            startHour->first,
            startMinute->first,
            endHour->first,
            endMinute->first
        };
        if (result.valid())
        {
            return result;
        }
    }
    return {};
}

std::string cellReference(int row, int column)
{
    std::string letters;
    int value = column;
    while (value > 0)
    {
        --value;
        letters.insert(
            letters.begin(),
            static_cast<char>('A' + value % 26)
            );
        value /= 26;
    }
    return letters + std::to_string(row);
}

const Cell* cellAt(
    const Sheet& sheet,
    const CellIndex& cells,
    int row,
    int column
    )
{
    const auto found = cells.find(positionKey(row, column));
    return found == cells.end() ? nullptr : &sheet.cells[found->second];
}

const Range* mergedRangeAt(
    const Sheet& sheet,
    int row,
    int column
    )
{
    for (const Range& range : sheet.mergedRanges)
    {
        if (range.contains(row, column))
        {
            return &range;
        }
    }
    return nullptr;
}

std::string weekdayFor(std::string_view value)
{
    const std::string normalized = upperAscii(simplifyAsciiWhitespace(value));
    if (normalized.find("MON") != std::string::npos
        || normalized.find("\xEC\x9B\x94") != std::string::npos)
    {
        return "Monday";
    }
    if (normalized.find("TUE") != std::string::npos
        || normalized.find("\xED\x99\x94") != std::string::npos)
    {
        return "Tuesday";
    }
    if (normalized.find("WED") != std::string::npos
        || normalized.find("\xEC\x88\x98") != std::string::npos)
    {
        return "Wednesday";
    }
    if (normalized.find("THU") != std::string::npos
        || normalized.find("\xEB\xAA\xA9") != std::string::npos)
    {
        return "Thursday";
    }
    if (normalized.find("FRI") != std::string::npos
        || normalized.find("\xEA\xB8\x88") != std::string::npos)
    {
        return "Friday";
    }
    return {};
}

std::uint32_t decodeUtf8(
    std::string_view value,
    std::size_t offset,
    std::size_t* length
    ) noexcept
{
    if (length)
    {
        *length = 0;
    }
    if (offset >= value.size())
    {
        return 0;
    }

    const auto byte = [&value](std::size_t index) {
        return static_cast<unsigned char>(value[index]);
    };
    const unsigned char first = byte(offset);
    std::uint32_t codePoint = 0;
    std::size_t count = 0;
    if (first < 0x80)
    {
        codePoint = first;
        count = 1;
    }
    else if ((first & 0xE0) == 0xC0)
    {
        codePoint = first & 0x1F;
        count = 2;
    }
    else if ((first & 0xF0) == 0xE0)
    {
        codePoint = first & 0x0F;
        count = 3;
    }
    else if ((first & 0xF8) == 0xF0)
    {
        codePoint = first & 0x07;
        count = 4;
    }
    else
    {
        return 0;
    }

    if (offset + count > value.size())
    {
        return 0;
    }
    for (std::size_t index = 1; index < count; ++index)
    {
        const unsigned char continuation = byte(offset + index);
        if ((continuation & 0xC0) != 0x80)
        {
            return 0;
        }
        codePoint = (codePoint << 6) | (continuation & 0x3F);
    }
    if (length)
    {
        *length = count;
    }
    return codePoint;
}

bool isHangul(std::uint32_t codePoint) noexcept
{
    return (codePoint >= 0x1100 && codePoint <= 0x11FF)
        || (codePoint >= 0x3130 && codePoint <= 0x318F)
        || (codePoint >= 0xA960 && codePoint <= 0xA97F)
        || (codePoint >= 0xAC00 && codePoint <= 0xD7AF)
        || (codePoint >= 0xD7B0 && codePoint <= 0xD7FF);
}

std::string hangulOnly(std::string_view value)
{
    std::string result;
    for (std::size_t offset = 0; offset < value.size();)
    {
        std::size_t length = 0;
        const std::uint32_t codePoint = decodeUtf8(value, offset, &length);
        if (length == 0)
        {
            ++offset;
            continue;
        }
        if (isHangul(codePoint))
        {
            result.append(value, offset, length);
        }
        offset += length;
    }
    return result;
}

std::string canonicalGrade(std::string_view value)
{
    const std::string candidate = trimAsciiWhitespace(value);
    for (const std::string& grade : ClassInfoConfig::grades())
    {
        if (equalsInsensitive(grade, candidate))
        {
            return grade;
        }
    }
    return {};
}

std::string normalizedApostrophe(std::string value)
{
    const std::string rightSingleQuote = "\xE2\x80\x99";
    std::size_t offset = 0;
    while ((offset = value.find(rightSingleQuote, offset))
           != std::string::npos)
    {
        value.replace(offset, rightSingleQuote.size(), "'");
        ++offset;
    }
    return value;
}

std::string canonicalLevel(
    std::string_view grade,
    std::string value
    )
{
    value = normalizedApostrophe(trimAsciiWhitespace(value));
    for (const std::string& level : ClassInfoConfig::levelsForGrade(grade))
    {
        if (equalsInsensitive(level, value))
        {
            return level;
        }
    }
    return {};
}

bool ignoredTimetableValue(std::string_view value)
{
    const std::string normalized = upperAscii(simplifyAsciiWhitespace(value));
    return normalized == "ESSAY"
        || normalized == "LUNCH"
        || normalized == "READY";
}

struct CourseMatch
{
    std::size_t start = 0;
    std::string grade;
    std::string level;
};

std::optional<CourseMatch> courseMatch(std::string_view value)
{
    for (std::size_t offset = 0; offset + 2 < value.size(); ++offset)
    {
        if (offset > 0 && isAsciiWord(value[offset - 1]))
        {
            continue;
        }
        const char first = static_cast<char>(
            std::toupper(static_cast<unsigned char>(value[offset]))
            );
        const char second = value[offset + 1];
        if ((first != 'E' && first != 'M')
            || !isAsciiDigit(second)
            || second < '1'
            || second > '6')
        {
            continue;
        }
        if (first == 'E' && second > '6')
        {
            continue;
        }
        if (first == 'M' && (second < '1' || second > '3'))
        {
            continue;
        }

        std::size_t cursor = skipAsciiWhitespace(value, offset + 2);
        std::size_t delimiterLength = 0;
        if (cursor < value.size() && value[cursor] == '-')
        {
            delimiterLength = 1;
        }
        else if (startsWithAt(value, cursor, "\xE2\x80\x93")
                 || startsWithAt(value, cursor, "\xE2\x80\x94"))
        {
            delimiterLength = 3;
        }
        if (delimiterLength == 0)
        {
            continue;
        }

        cursor = skipAsciiWhitespace(value, cursor + delimiterLength);
        const std::size_t levelStart = cursor;
        while (cursor < value.size() && isAsciiLetter(value[cursor]))
        {
            ++cursor;
        }
        if (cursor == levelStart)
        {
            continue;
        }
        if (startsWithAt(value, cursor, "'"))
        {
            ++cursor;
            const std::size_t apostropheEnd = cursor;
            while (cursor < value.size() && isAsciiLetter(value[cursor]))
            {
                ++cursor;
            }
            if (cursor == apostropheEnd)
            {
                continue;
            }
        }
        else if (startsWithAt(value, cursor, "\xE2\x80\x99"))
        {
            cursor += 3;
            const std::size_t apostropheEnd = cursor;
            while (cursor < value.size() && isAsciiLetter(value[cursor]))
            {
                ++cursor;
            }
            if (cursor == apostropheEnd)
            {
                continue;
            }
        }
        if (cursor < value.size() && isAsciiWord(value[cursor]))
        {
            continue;
        }

        return CourseMatch{
            offset,
            std::string{value.substr(offset, 2)},
            std::string{value.substr(levelStart, cursor - levelStart)}
        };
    }
    return std::nullopt;
}

std::string roomFromPrefix(std::string_view prefix)
{
    for (std::size_t offset = 0; offset < prefix.size(); ++offset)
    {
        if (prefix[offset] != '('
            && std::isspace(static_cast<unsigned char>(prefix[offset])) == 0)
        {
            continue;
        }

        std::size_t cursor = offset + 1;
        std::size_t start = cursor;
        if (cursor < prefix.size() && isAsciiLetter(prefix[cursor]))
        {
            ++cursor;
        }
        const std::size_t digitsStart = cursor;
        while (cursor < prefix.size() && isAsciiDigit(prefix[cursor]))
        {
            ++cursor;
        }
        const std::size_t digitCount = cursor - digitsStart;
        if (digitCount < 3 || digitCount > 4)
        {
            continue;
        }
        const std::size_t roomEnd = cursor;
        if (cursor < prefix.size() && prefix[cursor] == ')')
        {
            ++cursor;
        }
        return std::string(prefix.substr(start, roomEnd - start));
    }
    return {};
}

ParsedClassCell parseClassCell(std::string_view value)
{
    const std::optional<CourseMatch> course = courseMatch(value);
    if (!course)
    {
        return {};
    }

    const std::string grade = canonicalGrade(course->grade);
    const std::string level = canonicalLevel(grade, course->level);
    if (grade.empty() || level.empty())
    {
        return {};
    }

    const std::string prefix = std::string(value.substr(0, course->start));
    const std::string teacherKr = hangulOnly(prefix);
    const std::string room = roomFromPrefix(prefix);
    if (teacherKr.empty() || room.empty())
    {
        return {};
    }

    ParsedClassCell result;
    result.parsed = true;
    result.teacherKey = teacherKr;
    result.teacherKr = teacherKr;
    result.room = trimAsciiWhitespace(room);
    result.grade = grade;
    result.level = level;
    result.explicitTime = rawTimeRange(value);
    return result;
}

bool resolveTimetableTimes(
    std::vector<TimetableRow>* rows,
    ScheduleImportKind kind
    )
{
    if (!rows || rows->empty())
    {
        return false;
    }

    int noonTransition = -1;
    bool invalidDecrease = false;
    for (std::size_t index = 1; index < rows->size(); ++index)
    {
        if ((*rows)[index].raw.startHour
            >= (*rows)[index - 1].raw.startHour)
        {
            continue;
        }
        if (noonTransition < 0
            && (*rows)[index - 1].raw.startHour == 12
            && (*rows)[index].raw.startHour < 12)
        {
            noonTransition = static_cast<int>(index);
        }
        else
        {
            invalidDecrease = true;
        }
    }

    const bool ambiguousIntensive =
        kind == ScheduleImportKind::Intensive
        && (invalidDecrease
            || (noonTransition < 0 && rows->front().raw.startHour >= 9));
    if (ambiguousIntensive)
    {
        return true;
    }

    bool afterNoon = kind == ScheduleImportKind::Normal || noonTransition < 0;
    for (std::size_t index = 0; index < rows->size(); ++index)
    {
        TimetableRow& row = (*rows)[index];
        int hour = row.raw.startHour;
        if (kind == ScheduleImportKind::Normal)
        {
            if (hour != 12)
            {
                hour += 12;
            }
        }
        else
        {
            if (static_cast<int>(index) == noonTransition)
            {
                afterNoon = true;
            }
            if (afterNoon && hour != 12)
            {
                hour += 12;
            }
        }
        row.startMinutes = hour * 60 + row.raw.startMinute;

        const std::array<int, 3> candidates{
            row.raw.endHour * 60 + row.raw.endMinute,
            (row.raw.endHour + 12) * 60 + row.raw.endMinute,
            (row.raw.endHour + 24) * 60 + row.raw.endMinute
        };
        row.endMinutes = -1;
        for (const int candidate : candidates)
        {
            if (candidate > row.startMinutes
                && (row.endMinutes < 0 || candidate < row.endMinutes))
            {
                row.endMinutes = candidate;
            }
        }
    }
    return false;
}

std::string formattedTime(int minutes)
{
    if (minutes < 0)
    {
        return {};
    }
    const int normalized = minutes % (24 * 60);
    const int hour24 = normalized / 60;
    const int hour12 = hour24 % 12 == 0 ? 12 : hour24 % 12;
    std::ostringstream result;
    result << hour12 << ':';
    if (normalized % 60 < 10)
    {
        result << '0';
    }
    result << normalized % 60 << (hour24 < 12 ? " AM" : " PM");
    return result.str();
}

std::string intensiveSlotTime(int minutes)
{
    std::ostringstream result;
    if (minutes / 60 < 10)
    {
        result << '0';
    }
    result << minutes / 60 << ':';
    if (minutes % 60 < 10)
    {
        result << '0';
    }
    result << minutes % 60;
    return result.str();
}

void setIntensiveSlotState(
    std::vector<IntensiveSlotState>* states,
    std::string_view day,
    int startMinutes,
    std::string state
    )
{
    if (!states || startMinutes < 0)
    {
        return;
    }
    const std::string startTime = intensiveSlotTime(startMinutes);
    for (IntensiveSlotState& existing : *states)
    {
        if (existing.day == day && existing.startTime == startTime)
        {
            existing.state = std::move(state);
            return;
        }
    }
    states->push_back({std::string(day), startTime, std::move(state)});
}

void initializeIntensiveSlotStates(
    ScheduleImportUserBlock* result,
    const std::vector<std::string>& days
    )
{
    if (!result)
    {
        return;
    }
    for (const std::string& day : days)
    {
        if (day.empty())
        {
            continue;
        }
        for (int hour = IntensiveFirstHour;
             hour <= IntensiveFinalHour;
             ++hour)
        {
            setIntensiveSlotState(
                &result->intensiveSlotStates,
                day,
                hour * 60,
                "empty"
                );
        }
    }
}

std::string normalizedColor(std::string value)
{
    value = upperAscii(trimAsciiWhitespace(value));
    if (!value.empty() && value.front() == '#')
    {
        value.erase(value.begin());
    }
    if (value.size() == 8 && value.starts_with("FF"))
    {
        value.erase(0, 2);
    }
    return value;
}

std::string classCellColor(
    const Cell& cell,
    const std::vector<Style>& styles
    )
{
    if (cell.style < 0
        || static_cast<std::size_t>(cell.style) >= styles.size())
    {
        return {};
    }
    const Style& style = styles[static_cast<std::size_t>(cell.style)];
    std::string color = normalizedColor(style.fillColor);
    if (color.size() == 8)
    {
        color = color.substr(color.size() - 6);
    }
    return style.filled && color.size() == 6 ? '#' + color : std::string{};
}

bool sameTime(const ClassTime& left, const ClassTime& right)
{
    return left.day == right.day
        && left.startTime == right.startTime
        && left.endTime == right.endTime;
}

int occurrenceGroupScore(
    const std::vector<ParsedScheduleOccurrence>& occurrences,
    const std::vector<int>& indexes
    )
{
    if (indexes.empty())
    {
        return 0;
    }
    const ParsedScheduleOccurrence& first = occurrences[
        static_cast<std::size_t>(indexes.front())
        ];
    bool sameColor = !first.color.empty();
    bool sameTimeValue = true;
    bool sameRoom = true;
    for (const int index : indexes)
    {
        const ParsedScheduleOccurrence& occurrence = occurrences[
            static_cast<std::size_t>(index)
            ];
        sameColor = sameColor && occurrence.color == first.color;
        sameTimeValue = sameTimeValue
            && occurrence.time.startTime == first.time.startTime
            && occurrence.time.endTime == first.time.endTime;
        sameRoom = sameRoom && occurrence.room == first.room;
    }
    return (sameColor ? 1000 : 0)
        + (sameTimeValue ? 500 : 0)
        + (sameRoom ? 100 : 0);
}

OccurrencePartition bestOccurrencePartition(
    const std::vector<ParsedScheduleOccurrence>& occurrences,
    const std::vector<std::vector<std::string>>& allowedPatterns,
    const std::vector<int>& remaining
    )
{
    if (remaining.empty())
    {
        return {true, 0, {}};
    }

    const int firstIndex = remaining.front();
    const std::string firstDay = occurrences[
        static_cast<std::size_t>(firstIndex)
        ].time.day;
    std::vector<int> fallbackRemaining = remaining;
    fallbackRemaining.erase(fallbackRemaining.begin());
    const OccurrencePartition fallbackRest = bestOccurrencePartition(
        occurrences,
        allowedPatterns,
        fallbackRemaining
        );
    OccurrencePartition best;
    if (fallbackRest.valid)
    {
        best.valid = true;
        best.score = fallbackRest.score - 10'000;
        best.groups.push_back({firstIndex});
        best.groups.insert(
            best.groups.end(),
            fallbackRest.groups.begin(),
            fallbackRest.groups.end()
            );
    }

    for (const std::vector<std::string>& pattern : allowedPatterns)
    {
        if (std::find(pattern.begin(), pattern.end(), firstDay) == pattern.end())
        {
            continue;
        }
        std::vector<std::string> daysToChoose = pattern;
        const auto firstDayIt = std::find(
            daysToChoose.begin(),
            daysToChoose.end(),
            firstDay
            );
        daysToChoose.erase(firstDayIt);
        std::vector<int> selected{firstIndex};

        const auto choose = [&](const auto& self, std::size_t dayIndex) -> void {
            if (dayIndex >= daysToChoose.size())
            {
                std::vector<int> nextRemaining = remaining;
                for (const int index : selected)
                {
                    nextRemaining.erase(
                        std::remove(nextRemaining.begin(), nextRemaining.end(), index),
                        nextRemaining.end()
                        );
                }
                const OccurrencePartition rest = bestOccurrencePartition(
                    occurrences,
                    allowedPatterns,
                    nextRemaining
                    );
                if (!rest.valid)
                {
                    return;
                }
                const int score = occurrenceGroupScore(occurrences, selected)
                    + rest.score;
                if (!best.valid || score > best.score)
                {
                    best.valid = true;
                    best.score = score;
                    best.groups = {selected};
                    best.groups.insert(
                        best.groups.end(),
                        rest.groups.begin(),
                        rest.groups.end()
                        );
                }
                return;
            }

            const std::string& day = daysToChoose[dayIndex];
            for (const int index : remaining)
            {
                if (std::find(selected.begin(), selected.end(), index)
                        != selected.end()
                    || occurrences[static_cast<std::size_t>(index)].time.day
                        != day)
                {
                    continue;
                }
                selected.push_back(index);
                self(self, dayIndex + 1);
                selected.pop_back();
            }
        };
        choose(choose, 0);
    }
    return best;
}

ScheduleImportClassCandidate candidateForOccurrences(
    const std::vector<ParsedScheduleOccurrence>& occurrences,
    const std::vector<int>& indexes
    )
{
    ScheduleImportClassCandidate candidate;
    if (indexes.empty())
    {
        return candidate;
    }
    const ParsedScheduleOccurrence& first = occurrences[
        static_cast<std::size_t>(indexes.front())
        ];
    candidate.teacherKey = first.teacherKey;
    candidate.teacherKr = first.teacherKr;
    candidate.classGrade = first.classGrade;
    candidate.classLevel = first.classLevel;
    for (const int index : indexes)
    {
        const ParsedScheduleOccurrence& occurrence = occurrences[
            static_cast<std::size_t>(index)
            ];
        if (std::find(candidate.rooms.begin(), candidate.rooms.end(), occurrence.room)
            == candidate.rooms.end())
        {
            candidate.rooms.push_back(occurrence.room);
        }
        if (!occurrence.color.empty()
            && std::find(
                candidate.importedColors.begin(),
                candidate.importedColors.end(),
                occurrence.color
                ) == candidate.importedColors.end())
        {
            candidate.importedColors.push_back(occurrence.color);
        }
        if (std::none_of(
                candidate.times.begin(),
                candidate.times.end(),
                [&occurrence](const ClassTime& existing) {
                    return sameTime(existing, occurrence.time);
                }
                ))
        {
            candidate.times.push_back(occurrence.time);
        }
        if (std::find(
                candidate.sourceCells.begin(),
                candidate.sourceCells.end(),
                occurrence.sourceCell
                ) == candidate.sourceCells.end())
        {
            candidate.sourceCells.push_back(occurrence.sourceCell);
        }
    }

    const ScheduleImportMeetingPatternResult validation =
        ScheduleImportRules::validateMeetingPattern(candidate);
    if (validation.status
        == ScheduleImportMeetingPatternStatus::InvalidWeekdayOrDuplicate)
    {
        candidate.meetingPatternError =
            "Each imported class must have exactly one meeting per "
            "scheduled weekday.";
    }
    else if (validation.status
             == ScheduleImportMeetingPatternStatus::UnsupportedPattern)
    {
        std::string detected;
        for (const std::string& day : validation.meetingDays)
        {
            if (!detected.empty())
            {
                detected += ", ";
            }
            detected += day;
        }
        if (detected.empty())
        {
            detected = "no meetings";
        }
        std::string expectation;
        switch (validation.expectation)
        {
        case ScheduleImportMeetingPatternExpectation::WeekdayPairs:
            expectation =
                "Expected Monday/Wednesday, Monday/Friday, Wednesday/Friday, "
                "or Tuesday/Thursday.";
            break;
        case ScheduleImportMeetingPatternExpectation::
            WeekdayTripleOrTuesdayThursday:
            expectation =
                "Expected Monday/Wednesday/Friday or Tuesday/Thursday.";
            break;
        case ScheduleImportMeetingPatternExpectation::OneWeekday:
            expectation = "Expected one weekday meeting.";
            break;
        case ScheduleImportMeetingPatternExpectation::Unsupported:
            expectation =
                "The imported grade and level do not have a supported "
                "meeting-pattern rule.";
            break;
        }
        candidate.meetingPatternError = expectation
            + " Detected: " + detected + '.';
    }
    return candidate;
}

std::vector<ScheduleImportClassCandidate> aggregateOccurrences(
    const std::vector<ParsedScheduleOccurrence>& occurrences
    )
{
    std::unordered_map<std::string, std::vector<ParsedScheduleOccurrence>> grouped;
    std::vector<std::string> groupOrder;
    for (const ParsedScheduleOccurrence& occurrence : occurrences)
    {
        const std::string key = occurrence.teacherKey + '\x1f'
            + occurrence.classGrade + '\x1f' + occurrence.classLevel;
        if (!grouped.contains(key))
        {
            groupOrder.push_back(key);
        }
        grouped[key].push_back(occurrence);
    }

    std::vector<ScheduleImportClassCandidate> candidates;
    for (const std::string& key : groupOrder)
    {
        const std::vector<ParsedScheduleOccurrence>& group = grouped.at(key);
        const std::vector<std::vector<std::string>> patterns =
            ScheduleImportRules::allowedDayPatterns(
                group.front().classGrade,
                group.front().classLevel
                );
        std::vector<int> remaining;
        for (std::size_t index = 0; index < group.size(); ++index)
        {
            remaining.push_back(static_cast<int>(index));
        }
        if (patterns.empty())
        {
            candidates.push_back(candidateForOccurrences(group, remaining));
            continue;
        }

        const OccurrencePartition partition = bestOccurrencePartition(
            group,
            patterns,
            remaining
            );
        if (partition.valid)
        {
            for (const std::vector<int>& indexes : partition.groups)
            {
                candidates.push_back(candidateForOccurrences(group, indexes));
            }
        }
        else
        {
            candidates.push_back(candidateForOccurrences(group, remaining));
        }
    }
    return candidates;
}

std::string diagnosticMessage(
    std::string_view sheet,
    std::string_view cell,
    std::string_view message
    )
{
    return std::string(sheet) + '!' + std::string(cell) + ": "
        + std::string(message);
}

ScheduleImportUserBlock parseBlock(
    const Sheet& worksheet,
    const std::vector<Style>& styles,
    const CellIndex& cells,
    const Cell& header,
    ScheduleImportKind kind,
    const ScheduleWorkbookCancellation& isCancelled
    )
{
    ScheduleImportUserBlock result;
    result.name = simplifyAsciiWhitespace(header.value);
    result.headerCell = cellReference(header.row, header.column);

    std::vector<std::string> days;
    days.reserve(5);
    for (int offset = 1; offset <= 5; ++offset)
    {
        const Cell* dayCell = cellAt(
            worksheet,
            cells,
            header.row,
            header.column + offset
            );
        days.push_back(dayCell ? weekdayFor(dayCell->value) : std::string{});
    }

    std::vector<TimetableRow> timetableRows;
    for (int row = header.row + 1; row <= header.row + 20; ++row)
    {
        if (isCancelled && isCancelled())
        {
            return result;
        }
        const Cell* timeCell = cellAt(
            worksheet,
            cells,
            row,
            header.column
            );
        if (!timeCell)
        {
            break;
        }
        const RawTimeRange raw = rawTimeRange(timeCell->value);
        if (!raw.valid())
        {
            break;
        }
        timetableRows.push_back({row, raw});
    }

    if (resolveTimetableTimes(&timetableRows, kind))
    {
        for (const TimetableRow& row : timetableRows)
        {
            if (isCancelled && isCancelled())
            {
                return result;
            }
            for (std::size_t dayOffset = 0; dayOffset < days.size(); ++dayOffset)
            {
                const int column = header.column
                    + static_cast<int>(dayOffset) + 1;
                const Cell* cell = cellAt(worksheet, cells, row.row, column);
                if (cell && parseClassCell(cell->value).parsed)
                {
                    result.diagnostics.push_back({
                        worksheet.name,
                        result.name,
                        cellReference(row.row, column),
                        cell->value,
                        "The intensive schedule's AM-to-PM transition is ambiguous."
                    });
                }
            }
        }
        return result;
    }

    std::unordered_map<int, TimetableRow> timeByRow;
    for (const TimetableRow& row : timetableRows)
    {
        if (isCancelled && isCancelled())
        {
            return result;
        }
        timeByRow.insert_or_assign(row.row, row);
    }

    if (kind == ScheduleImportKind::Intensive)
    {
        initializeIntensiveSlotStates(&result, days);
        for (const TimetableRow& row : timetableRows)
        {
            for (std::size_t dayOffset = 0; dayOffset < days.size(); ++dayOffset)
            {
                const std::string& day = days[dayOffset];
                if (day.empty())
                {
                    continue;
                }
                const int column = header.column
                    + static_cast<int>(dayOffset) + 1;
                const Cell* cell = cellAt(worksheet, cells, row.row, column);
                if (!cell)
                {
                    if (const Range* range = mergedRangeAt(
                            worksheet,
                            row.row,
                            column
                            ))
                    {
                        cell = cellAt(
                            worksheet,
                            cells,
                            range->firstRow,
                            range->firstColumn
                            );
                    }
                }
                const std::string value = cell
                    ? trimAsciiWhitespace(cell->value)
                    : std::string{};
                const std::string state = value.empty()
                    ? "empty"
                    : equalsInsensitive(value, "Lunch")
                        ? "lunch"
                        : "essay";
                setIntensiveSlotState(
                    &result.intensiveSlotStates,
                    day,
                    row.startMinutes,
                    state
                    );
            }
        }
    }

    std::vector<ParsedScheduleOccurrence> occurrences;
    for (const TimetableRow& row : timetableRows)
    {
        for (std::size_t dayOffset = 0; dayOffset < days.size(); ++dayOffset)
        {
            if (isCancelled && isCancelled())
            {
                return result;
            }
            if (days[dayOffset].empty())
            {
                continue;
            }
            const int column = header.column
                + static_cast<int>(dayOffset) + 1;
            const Cell* cell = cellAt(worksheet, cells, row.row, column);
            if (!cell || trimAsciiWhitespace(cell->value).empty()
                || ignoredTimetableValue(cell->value))
            {
                continue;
            }

            const ParsedClassCell parsed = parseClassCell(cell->value);
            const std::string reference = cellReference(row.row, column);
            if (!parsed.parsed)
            {
                result.diagnostics.push_back({
                    worksheet.name,
                    result.name,
                    reference,
                    cell->value,
                    "This occupied timetable cell was not recognized as a class."
                });
                continue;
            }

            int startMinutes = row.startMinutes;
            int endMinutes = row.endMinutes;
            if (parsed.explicitTime.valid())
            {
                const std::array<int, 3> startCandidates{
                    parsed.explicitTime.startHour * 60
                        + parsed.explicitTime.startMinute,
                    (parsed.explicitTime.startHour + 12) * 60
                        + parsed.explicitTime.startMinute,
                    (parsed.explicitTime.startHour + 24) * 60
                        + parsed.explicitTime.startMinute
                };
                int explicitStart = startCandidates.front();
                for (const int candidate : startCandidates)
                {
                    if (std::abs(candidate - row.startMinutes)
                        < std::abs(explicitStart - row.startMinutes))
                    {
                        explicitStart = candidate;
                    }
                }
                const std::array<int, 3> endCandidates{
                    parsed.explicitTime.endHour * 60
                        + parsed.explicitTime.endMinute,
                    (parsed.explicitTime.endHour + 12) * 60
                        + parsed.explicitTime.endMinute,
                    (parsed.explicitTime.endHour + 24) * 60
                        + parsed.explicitTime.endMinute
                };
                for (const int candidate : endCandidates)
                {
                    if (candidate > explicitStart)
                    {
                        startMinutes = explicitStart;
                        endMinutes = candidate;
                        break;
                    }
                }
            }
            else if (const Range* range = mergedRangeAt(
                         worksheet,
                         row.row,
                         column
                         ))
            {
                const auto timeRow = timeByRow.find(range->lastRow);
                if (timeRow != timeByRow.end())
                {
                    endMinutes = timeRow->second.endMinutes;
                }
            }

            if (startMinutes < 0 || endMinutes <= startMinutes)
            {
                result.diagnostics.push_back({
                    worksheet.name,
                    result.name,
                    reference,
                    cell->value,
                    "The class time could not be interpreted."
                });
                continue;
            }

            ClassTime time{
                days[dayOffset],
                formattedTime(startMinutes),
                formattedTime(endMinutes)
            };
            const bool duplicate = std::any_of(
                occurrences.begin(),
                occurrences.end(),
                [&parsed, &time](const ParsedScheduleOccurrence& existing) {
                    return existing.teacherKey == parsed.teacherKey
                        && existing.classGrade == parsed.grade
                        && existing.classLevel == parsed.level
                        && sameTime(existing.time, time);
                }
                );
            if (duplicate)
            {
                continue;
            }
            occurrences.push_back({
                parsed.teacherKey,
                parsed.teacherKr,
                parsed.room,
                classCellColor(*cell, styles),
                parsed.grade,
                parsed.level,
                std::move(time),
                reference
            });
        }
    }
    result.classes = aggregateOccurrences(occurrences);
    return result;
}

bool validHeader(const Cell& candidate, const CellIndex& cells, const Sheet& sheet)
{
    if (trimAsciiWhitespace(candidate.value).empty()
        || rawTimeRange(candidate.value).valid())
    {
        return false;
    }
    for (int offset = 1; offset <= 5; ++offset)
    {
        const Cell* day = cellAt(
            sheet,
            cells,
            candidate.row,
            candidate.column + offset
            );
        if (!day || weekdayFor(day->value).empty())
        {
            return false;
        }
    }
    return true;
}

Error makeError(ErrorCode code, std::string message)
{
    return Error{code, std::move(message), std::nullopt};
}
} // namespace

Result<ScheduleImportWorkbook> ScheduleWorkbookInterpreter::interpret(
    const ScheduleWorkbookLayout& layout,
    ScheduleImportKind kind,
    const ScheduleWorkbookCancellation& isCancelled
    )
{
    const auto cancelled = [&isCancelled] {
        return isCancelled && isCancelled();
    };
    if (cancelled())
    {
        return std::unexpected(makeError(
            ErrorCode::Cancelled,
            "The schedule import was cancelled."
            ));
    }
    if (layout.sheets.empty())
    {
        return std::unexpected(makeError(
            ErrorCode::InvalidFormat,
            "The workbook could not be read."
            ));
    }

    ScheduleImportWorkbook result;
    result.sheets.reserve(layout.sheets.size());
    for (const Sheet& worksheet : layout.sheets)
    {
        if (cancelled())
        {
            return std::unexpected(makeError(
                ErrorCode::Cancelled,
                "The schedule import was cancelled."
                ));
        }

        ScheduleImportSheet sheet;
        sheet.name = worksheet.name;
        sheet.visible = worksheet.visible;
        CellIndex cells;
        cells.reserve(worksheet.cells.size());
        for (std::size_t index = 0; index < worksheet.cells.size(); ++index)
        {
            if (cancelled())
            {
                return std::unexpected(makeError(
                    ErrorCode::Cancelled,
                    "The schedule import was cancelled."
                    ));
            }
            cells.insert_or_assign(
                positionKey(
                    worksheet.cells[index].row,
                    worksheet.cells[index].column
                    ),
                index
                );
        }

        for (const Cell& cell : worksheet.cells)
        {
            if (!validHeader(cell, cells, worksheet))
            {
                continue;
            }
            ScheduleImportUserBlock block = parseBlock(
                worksheet,
                layout.styles,
                cells,
                cell,
                kind,
                isCancelled
                );
            if (cancelled())
            {
                return std::unexpected(makeError(
                    ErrorCode::Cancelled,
                    "The schedule import was cancelled."
                    ));
            }
            if (!block.classes.empty())
            {
                sheet.users.push_back(std::move(block));
            }
            else if (!block.diagnostics.empty())
            {
                sheet.diagnostics.insert(
                    sheet.diagnostics.end(),
                    block.diagnostics.begin(),
                    block.diagnostics.end()
                    );
            }
        }
        result.sheets.push_back(std::move(sheet));
    }

    const bool hasVisibleSchedule = std::any_of(
        result.sheets.begin(),
        result.sheets.end(),
        [](const ScheduleImportSheet& sheet) {
            return sheet.visible && !sheet.users.empty();
        }
        );
    if (!hasVisibleSchedule)
    {
        for (const ScheduleImportSheet& sheet : result.sheets)
        {
            if (!sheet.visible || sheet.diagnostics.empty())
            {
                continue;
            }
            const ScheduleImportDiagnostic& diagnostic = sheet.diagnostics.front();
            return std::unexpected(makeError(
                ErrorCode::InvalidFormat,
                diagnosticMessage(
                    sheet.name,
                    diagnostic.cellReference,
                    diagnostic.message
                    )
                ));
        }
        return std::unexpected(makeError(
            ErrorCode::NotFound,
            "No supported user schedule blocks were found in the workbook."
            ));
    }
    return result;
}

} // namespace classmngr::engine
