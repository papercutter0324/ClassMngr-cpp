#include "classmngr/engine/schedule_builder.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace classmngr::engine
{
namespace
{
constexpr int DefaultStartHour = 16;
constexpr int FinalHour = 21;
constexpr int FullIntensiveStartHour = 9;
constexpr int FullIntensiveFinalHour = 21;

struct ParsedClass
{
    std::string day;
    int startMinutes = 0;
    ScheduleReportEntry entry;
};

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

bool isDigit(char value)
{
    return value >= '0' && value <= '9';
}

char upperAscii(char value)
{
    if (value >= 'a' && value <= 'z')
    {
        return static_cast<char>(value - 'a' + 'A');
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
        if (upperAscii(left[index]) != upperAscii(right[index]))
        {
            return false;
        }
    }

    return true;
}

bool allDigits(std::string_view value)
{
    if (value.empty())
    {
        return false;
    }

    for (const char character : value)
    {
        if (!isDigit(character))
        {
            return false;
        }
    }

    return true;
}

std::optional<int> parseClock(std::string_view value)
{
    const std::string normalized = trimAsciiWhitespace(value);
    const std::size_t colon = normalized.find(':');
    if (colon == std::string::npos
        || colon == 0
        || colon > 2
        || !allDigits(std::string_view(normalized).substr(0, colon))
        || colon + 3 > normalized.size()
        || !isDigit(normalized[colon + 1])
        || !isDigit(normalized[colon + 2]))
    {
        return std::nullopt;
    }

    const int hour = colon == 1
        ? normalized[0] - '0'
        : ((normalized[0] - '0') * 10) + normalized[1] - '0';
    const int minute =
        ((normalized[colon + 1] - '0') * 10)
        + normalized[colon + 2] - '0';
    if (minute > 59)
    {
        return std::nullopt;
    }

    const std::string_view suffix =
        std::string_view(normalized).substr(colon + 3);
    if (suffix.empty() || suffix.front() == ' ')
    {
        const std::string_view timeSuffix =
            suffix.empty() ? suffix : suffix.substr(1);
        if (timeSuffix.empty())
        {
            if (hour > 23 || normalized.size() != colon + 3)
            {
                return std::nullopt;
            }

            return (hour * 60) + minute;
        }

        if (timeSuffix.size() != 2
            || (!equalsAsciiInsensitive(timeSuffix, "AM")
                && !equalsAsciiInsensitive(timeSuffix, "PM")))
        {
            return std::nullopt;
        }

        if (hour < 1 || hour > 12)
        {
            return std::nullopt;
        }

        const int hour24 =
            (hour % 12)
            + (equalsAsciiInsensitive(timeSuffix, "PM") ? 12 : 0);
        return (hour24 * 60) + minute;
    }

    if (suffix.size() == 2
        && (equalsAsciiInsensitive(suffix, "AM")
            || equalsAsciiInsensitive(suffix, "PM")))
    {
        if (hour < 1 || hour > 12)
        {
            return std::nullopt;
        }

        const int hour24 =
            (hour % 12)
            + (equalsAsciiInsensitive(suffix, "PM") ? 12 : 0);
        return (hour24 * 60) + minute;
    }

    if (suffix.size() != 3
        || suffix.front() != ':'
        || !isDigit(suffix[1])
        || !isDigit(suffix[2])
        || suffix[1] > '5'
        || hour > 23)
    {
        return std::nullopt;
    }

    return (hour * 60) + minute;
}

ScheduleReportEntry toEntry(const ClassInfo& info)
{
    ScheduleReportEntry entry;
    entry.classId = info.classId;
    entry.teacherKr = info.teacherKr;
    entry.teacherEn = info.teacherEn;
    entry.teacherPreferredName = info.teacherPreferredName;
    entry.roomNumber = info.roomNumber;
    entry.classGrade = info.classGrade;
    entry.classLevel = info.classLevel;
    entry.classColor = info.classColor.empty()
        ? "#FFFFFF"
        : info.classColor;
    entry.fontColor = info.fontColor.empty()
        ? "#000000"
        : info.fontColor;
    return entry;
}

std::string timeLabel(int minutes)
{
    const int hour = minutes / 60;
    const int minute = minutes % 60;

    std::string result = hour < 10
        ? "0" + std::to_string(hour)
        : std::to_string(hour);
    result += ':';
    result += minute < 10
        ? "0" + std::to_string(minute)
        : std::to_string(minute);
    return result;
}

std::vector<ScheduleReportRow> buildRows(
    int startHour,
    int finalHour,
    int offset
    )
{
    std::vector<ScheduleReportRow> rows;
    for (int hour = startHour; hour <= finalHour; ++hour)
    {
        int displayHour = hour;
        if (offset == 55)
        {
            --displayHour;
        }

        rows.push_back({
            timeLabel((displayHour * 60) + offset)
        });
    }
    return rows;
}
} // namespace

ScheduleReportBuildResult ScheduleBuilderService::build(
    const std::vector<ClassInfo>& classInfos,
    bool useIntensive,
    const std::vector<std::string>& visibleDays
    )
{
    ScheduleReportBuildResult result;
    result.days = visibleDays;
    for (const std::string& day : visibleDays)
    {
        result.schedule.emplace(day, std::map<std::string,
            std::vector<ScheduleReportEntry>>{});
    }

    std::vector<ParsedClass> parsedClasses;
    bool hasEarliestHour = false;
    int earliestHour = 0;
    int scheduleOffset = 0;

    for (const ClassInfo& info : classInfos)
    {
        const std::vector<ClassTime>& times =
            useIntensive ? info.intensiveTimes : info.classTimes;

        for (const ClassTime& time : times)
        {
            const std::string trimmedDay = trimAsciiWhitespace(time.day);
            const std::string day = trimmedDay.empty()
                ? "Monday"
                : trimmedDay;
            if (std::find(
                    visibleDays.begin(),
                    visibleDays.end(),
                    day
                    ) == visibleDays.end())
            {
                continue;
            }

            const std::optional<int> start = parseClock(time.startTime);
            if (!start)
            {
                continue;
            }

            const std::optional<int> end = parseClock(time.endTime);
            const int startHour = *start / 60
                + (*start % 60 == 55 ? 1 : 0);
            if (!hasEarliestHour || startHour < earliestHour)
            {
                earliestHour = startHour;
                hasEarliestHour = true;
            }

            if (end && *end % 60 == 55)
            {
                result.uses55Endings = true;
            }

            if (*start % 60 == 55)
            {
                scheduleOffset = 55;
            }
            else if (*start % 60 == 5 && scheduleOffset != 55)
            {
                scheduleOffset = 5;
            }

            parsedClasses.push_back({
                day,
                *start,
                toEntry(info)
            });
        }
    }

    const int startHour = useIntensive
        ? FullIntensiveStartHour
        : hasEarliestHour && earliestHour < DefaultStartHour
            ? earliestHour
            : DefaultStartHour;
    const int finalHour = useIntensive
        ? FullIntensiveFinalHour
        : FinalHour;

    if (useIntensive)
    {
        scheduleOffset = 0;
        result.uses55Endings = false;
    }

    result.scheduleOffset = scheduleOffset;
    result.rows = buildRows(startHour, finalHour, scheduleOffset);

    for (const ParsedClass& parsedClass : parsedClasses)
    {
        result.schedule[parsedClass.day][timeLabel(parsedClass.startMinutes)]
            .push_back(parsedClass.entry);
    }

    return result;
}

} // namespace classmngr::engine
