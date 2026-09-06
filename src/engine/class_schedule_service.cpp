#include "classmngr/engine/class_schedule_service.h"

#include "classmngr/engine/class_info_config.h"
#include "classmngr/engine/sqlite_database.h"

#include <cctype>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace classmngr::engine
{
namespace
{
constexpr int MinutesPerDay = 24 * 60;
constexpr int MinutesPerWeek = 7 * MinutesPerDay;

struct TimeInterval
{
    int start = -1;
    int end = -1;
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

char upperAscii(char value)
{
    return static_cast<char>(
        std::toupper(static_cast<unsigned char>(value))
        );
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

int dayIndex(std::string_view value)
{
    const std::string day = trimAsciiWhitespace(value);
    const auto& days = ClassInfoConfig::days();
    for (std::size_t index = 0; index < days.size(); ++index)
    {
        if (equalsAsciiInsensitive(day, days[index]))
        {
            return static_cast<int>(index);
        }
    }
    return -1;
}

bool isDigit(char value)
{
    return value >= '0' && value <= '9';
}

int twoDigits(std::string_view value, std::size_t offset)
{
    return (value[offset] - '0') * 10 + value[offset + 1] - '0';
}

std::optional<int> parseClock(std::string_view value)
{
    const std::string normalized = trimAsciiWhitespace(value);
    if (normalized.size() == 5
        && normalized[2] == ':'
        && isDigit(normalized[0])
        && isDigit(normalized[1])
        && isDigit(normalized[3])
        && isDigit(normalized[4]))
    {
        const int hour = twoDigits(normalized, 0);
        const int minute = twoDigits(normalized, 3);
        if (hour <= 23 && minute <= 59)
        {
            return hour * 60 + minute;
        }
    }

    const std::size_t colon = normalized.find(':');
    if (colon == std::string::npos
        || colon == 0
        || colon > 2
        || normalized.size() != colon + 6
        || normalized[colon + 3] != ' '
        || !isDigit(normalized[colon + 1])
        || !isDigit(normalized[colon + 2]))
    {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < colon; ++index)
    {
        if (!isDigit(normalized[index]))
        {
            return std::nullopt;
        }
    }

    const char period = upperAscii(normalized[colon + 4]);
    if ((period != 'A' && period != 'P')
        || upperAscii(normalized[colon + 5]) != 'M')
    {
        return std::nullopt;
    }
    if (colon == 2 && normalized[0] == '0')
    {
        return std::nullopt;
    }

    const int hour = colon == 1
        ? normalized[0] - '0'
        : twoDigits(normalized, 0);
    const int minute = twoDigits(normalized, colon + 1);
    if (hour < 1 || hour > 12 || minute > 59)
    {
        return std::nullopt;
    }

    int hour24 = hour % 12;
    if (period == 'P')
    {
        hour24 += 12;
    }
    return hour24 * 60 + minute;
}

std::optional<TimeInterval> toInterval(const ClassTime& time)
{
    const int day = dayIndex(time.day);
    const std::optional<int> start = parseClock(time.startTime);
    const std::optional<int> end = parseClock(time.endTime);
    if (day < 0 || !start || !end)
    {
        return std::nullopt;
    }

    TimeInterval interval;
    interval.start = day * MinutesPerDay + *start;
    interval.end = day * MinutesPerDay + *end;
    if (interval.end <= interval.start)
    {
        interval.end += MinutesPerDay;
    }
    return interval;
}

bool intervalsOverlap(
    const TimeInterval& first,
    const TimeInterval& second
    )
{
    for (const int offset : {-MinutesPerWeek, 0, MinutesPerWeek})
    {
        const int secondStart = second.start + offset;
        const int secondEnd = second.end + offset;
        if (first.start < secondEnd && secondStart < first.end)
        {
            return true;
        }
    }
    return false;
}

std::string classDisplayName(
    std::string_view name,
    int classId
    )
{
    const std::string trimmed = trimAsciiWhitespace(name);
    return trimmed.empty()
        ? "Class " + std::to_string(classId)
        : trimmed;
}

Result<std::string> textValue(
    const SqliteValue& value,
    std::string_view column
    )
{
    if (const auto* text = std::get_if<std::string>(&value))
    {
        return *text;
    }
    if (std::holds_alternative<std::monostate>(value))
    {
        return std::string{};
    }
    return std::unexpected(error(
        ErrorCode::Schema,
        "SQLite returned a non-text schedule " + std::string(column)
            + " value."
        ));
}

Result<int> integerValue(
    const SqliteValue& value,
    std::string_view column,
    int nullValue = -1
    )
{
    if (std::holds_alternative<std::monostate>(value))
    {
        return nullValue;
    }
    const auto* integer = std::get_if<std::int64_t>(&value);
    if (integer == nullptr
        || *integer < std::numeric_limits<int>::min()
        || *integer > std::numeric_limits<int>::max())
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an invalid schedule " + std::string(column)
                + " value."
            ));
    }
    return static_cast<int>(*integer);
}

Result<bool> classExists(
    SqliteDatabase& database,
    int classId
    )
{
    const auto rows = database.query(
        "SELECT EXISTS(SELECT 1 FROM classes WHERE id=?)",
        SqliteParameters{SqliteValue{std::int64_t{classId}}}
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }
    if (rows->rows.size() != 1 || rows->rows.front().values.size() != 1)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an unexpected class existence result."
            ));
    }
    const auto* value = std::get_if<std::int64_t>(
        &rows->rows.front().values.front()
        );
    if (value == nullptr)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned a non-integer class existence result."
            ));
    }
    return *value != 0;
}

Status loadScheduleTimes(
    SqliteDatabase& database,
    std::string_view table,
    std::map<int, std::size_t>& indexesByClassId,
    std::vector<ClassInfo>& infos,
    bool intensive
    )
{
    const auto rows = database.query(
        std::string("SELECT times.class_id, times.day, times.start_time, ")
            + "times.end_time FROM " + std::string(table)
            + " times INNER JOIN classes c ON c.id=times.class_id "
              "LEFT JOIN testing_classes tc ON tc.class_id=c.id "
              "WHERE tc.class_id IS NULL ORDER BY c.name, c.id, times.id"
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }

    for (const SqliteRow& row : rows->rows)
    {
        if (row.values.size() != 4)
        {
            return std::unexpected(error(
                ErrorCode::Schema,
                "SQLite returned an unexpected schedule time row shape."
                ));
        }
        const Result<int> classId = integerValue(row.values[0], "class_id");
        if (!classId)
        {
            return std::unexpected(classId.error());
        }
        const auto found = indexesByClassId.find(*classId);
        if (found == indexesByClassId.end())
        {
            continue;
        }
        const Result<std::string> day = textValue(row.values[1], "day");
        const Result<std::string> start = textValue(row.values[2], "start_time");
        const Result<std::string> end = textValue(row.values[3], "end_time");
        if (!day)
        {
            return std::unexpected(day.error());
        }
        if (!start)
        {
            return std::unexpected(start.error());
        }
        if (!end)
        {
            return std::unexpected(end.error());
        }
        ClassTime time{*day, *start, *end};
        if (intensive)
        {
            infos[found->second].intensiveTimes.push_back(std::move(time));
        }
        else
        {
            infos[found->second].classTimes.push_back(std::move(time));
        }
    }
    return {};
}

Result<std::string> className(
    SqliteDatabase& database,
    int classId
    )
{
    const auto rows = database.query(
        "SELECT name FROM classes WHERE id=?",
        SqliteParameters{SqliteValue{std::int64_t{classId}}}
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }
    if (rows->rows.empty())
    {
        return std::unexpected(error(
            ErrorCode::NotFound,
            "No class exists for id " + std::to_string(classId) + "."
            ));
    }
    if (rows->rows.front().values.size() != 1)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an unexpected class name row shape."
            ));
    }
    return textValue(rows->rows.front().values.front(), "name");
}
} // namespace

ClassScheduleService::ClassScheduleService(SqliteDatabase& database)
    : m_database(database)
{
}

std::vector<ClassConflict> ClassScheduleService::findConflicts(
    const std::vector<ClassScheduleEntry>& candidates,
    const std::vector<ClassScheduleEntry>& existing
    ) const
{
    std::vector<std::optional<TimeInterval>> candidateIntervals;
    candidateIntervals.reserve(candidates.size());
    for (const ClassScheduleEntry& candidate : candidates)
    {
        candidateIntervals.push_back(toInterval(candidate.time));
    }

    std::vector<std::optional<TimeInterval>> existingIntervals;
    existingIntervals.reserve(existing.size());
    for (const ClassScheduleEntry& entry : existing)
    {
        existingIntervals.push_back(toInterval(entry.time));
    }

    std::vector<ClassConflict> conflicts;
    for (std::size_t first = 0; first < candidates.size(); ++first)
    {
        if (!candidateIntervals[first])
        {
            continue;
        }

        const auto appendConflict = [&conflicts, &candidates, first](
            const ClassScheduleEntry& other
            )
        {
            conflicts.push_back({
                candidates[first].classId,
                classDisplayName(
                    candidates[first].className,
                    candidates[first].classId
                    ),
                candidates[first].time.day,
                candidates[first].time.startTime,
                candidates[first].time.endTime,
                classDisplayName(other.className, other.classId)
            });
        };

        for (std::size_t second = first + 1;
             second < candidates.size();
             ++second)
        {
            if (candidateIntervals[second]
                && intervalsOverlap(
                    *candidateIntervals[first],
                    *candidateIntervals[second]
                    ))
            {
                appendConflict(candidates[second]);
            }
        }

        for (std::size_t second = 0; second < existing.size(); ++second)
        {
            if (existingIntervals[second]
                && intervalsOverlap(
                    *candidateIntervals[first],
                    *existingIntervals[second]
                    ))
            {
                appendConflict(existing[second]);
            }
        }
    }

    return conflicts;
}

Result<std::vector<ClassTeacherAssignment>>
ClassScheduleService::loadClassTeacherAssignments()
{
    const auto rows = m_database.query(
        "SELECT c.id, ci.teacher_id FROM classes c "
        "LEFT JOIN testing_classes tc ON tc.class_id=c.id "
        "LEFT JOIN class_info ci ON ci.class_id=c.id "
        "WHERE tc.class_id IS NULL ORDER BY c.name, c.id"
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }

    std::vector<ClassTeacherAssignment> assignments;
    assignments.reserve(rows->rows.size());
    for (const SqliteRow& row : rows->rows)
    {
        if (row.values.size() != 2)
        {
            return std::unexpected(error(
                ErrorCode::Schema,
                "SQLite returned an unexpected class assignment row shape."
                ));
        }
        const Result<int> classId = integerValue(row.values[0], "class_id");
        const Result<int> teacherId = integerValue(row.values[1], "teacher_id");
        if (!classId)
        {
            return std::unexpected(classId.error());
        }
        if (!teacherId)
        {
            return std::unexpected(teacherId.error());
        }
        assignments.push_back({*classId, *teacherId});
    }
    return assignments;
}

Result<std::vector<ClassInfo>> ClassScheduleService::loadScheduleClassInfos()
{
    const auto rows = m_database.query(
        "SELECT c.id, ci.teacher_id, ci.class_grade, ci.class_level, "
        "ci.class_color, ci.font_color, t.teacher_kr, t.teacher_en, "
        "t.preferred_name, t.room_number FROM classes c "
        "LEFT JOIN testing_classes tc ON tc.class_id=c.id "
        "LEFT JOIN class_info ci ON ci.class_id=c.id "
        "LEFT JOIN teachers t ON t.id=ci.teacher_id "
        "WHERE tc.class_id IS NULL ORDER BY c.name, c.id"
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }

    std::vector<ClassInfo> infos;
    std::map<int, std::size_t> indexesByClassId;
    infos.reserve(rows->rows.size());
    for (const SqliteRow& row : rows->rows)
    {
        if (row.values.size() != 10)
        {
            return std::unexpected(error(
                ErrorCode::Schema,
                "SQLite returned an unexpected schedule class row shape."
                ));
        }

        const Result<int> classId = integerValue(row.values[0], "class_id");
        const Result<int> teacherId = integerValue(row.values[1], "teacher_id");
        if (!classId)
        {
            return std::unexpected(classId.error());
        }
        if (!teacherId)
        {
            return std::unexpected(teacherId.error());
        }

        ClassInfo info;
        info.classId = *classId;
        info.teacherId = *teacherId;
        const Result<std::string> grade = textValue(row.values[2], "class_grade");
        const Result<std::string> level = textValue(row.values[3], "class_level");
        const Result<std::string> classColor = textValue(row.values[4], "class_color");
        const Result<std::string> fontColor = textValue(row.values[5], "font_color");
        if (!grade || !level || !classColor || !fontColor)
        {
            if (!grade) return std::unexpected(grade.error());
            if (!level) return std::unexpected(level.error());
            if (!classColor) return std::unexpected(classColor.error());
            return std::unexpected(fontColor.error());
        }
        info.classGrade = *grade;
        info.classLevel = *level;
        if (!classColor->empty())
        {
            info.classColor = *classColor;
        }
        if (!fontColor->empty())
        {
            info.fontColor = *fontColor;
        }

        std::string* const teacherFields[] = {
            &info.teacherKr,
            &info.teacherEn,
            &info.teacherPreferredName,
            &info.roomNumber
        };
        const std::string_view teacherColumns[] = {
            "teacher_kr", "teacher_en", "preferred_name", "room_number"
        };
        for (std::size_t index = 0; index < 4; ++index)
        {
            const Result<std::string> value = textValue(
                row.values[index + 6],
                teacherColumns[index]
                );
            if (!value)
            {
                return std::unexpected(value.error());
            }
            *teacherFields[index] = *value;
        }

        indexesByClassId.emplace(info.classId, infos.size());
        infos.push_back(std::move(info));
    }

    const Status regular = loadScheduleTimes(
        m_database,
        "class_times",
        indexesByClassId,
        infos,
        false
        );
    if (!regular)
    {
        return std::unexpected(regular.error());
    }
    const Status intensive = loadScheduleTimes(
        m_database,
        "class_intensive_times",
        indexesByClassId,
        infos,
        true
        );
    if (!intensive)
    {
        return std::unexpected(intensive.error());
    }
    return infos;
}

Result<std::vector<ClassConflict>> ClassScheduleService::getClassTimeConflicts(
    int classId,
    const std::vector<ClassTime>& times,
    ScheduleType type
    )
{
    if (classId <= 0)
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "Loading class time conflicts requires a positive class id."
            ));
    }

    const Result<bool> present = classExists(m_database, classId);
    if (!present)
    {
        return std::unexpected(present.error());
    }
    if (!*present)
    {
        return std::unexpected(error(
            ErrorCode::NotFound,
            "No class exists for id " + std::to_string(classId) + "."
            ));
    }
    const Result<std::string> currentNameValue = className(m_database, classId);
    if (!currentNameValue)
    {
        return std::unexpected(currentNameValue.error());
    }
    const std::string currentName = classDisplayName(*currentNameValue, classId);

    std::vector<std::optional<TimeInterval>> candidateIntervals;
    candidateIntervals.reserve(times.size());
    for (const ClassTime& time : times)
    {
        candidateIntervals.push_back(toInterval(time));
    }

    std::vector<ClassConflict> conflicts;
    for (std::size_t first = 0; first < times.size(); ++first)
    {
        if (!candidateIntervals[first])
        {
            continue;
        }
        for (std::size_t second = first + 1; second < times.size(); ++second)
        {
            if (candidateIntervals[second]
                && intervalsOverlap(*candidateIntervals[first], *candidateIntervals[second]))
            {
                conflicts.push_back({
                    classId,
                    currentName,
                    times[first].day,
                    times[first].startTime,
                    times[first].endTime,
                    currentName
                });
            }
        }
    }

    const std::string_view table = type == ScheduleType::Regular
        ? "class_times"
        : "class_intensive_times";
    const auto rows = m_database.query(
        std::string("SELECT times.class_id, classes.name, times.day, ")
            + "times.start_time, times.end_time FROM " + std::string(table)
            + " times LEFT JOIN classes ON classes.id=times.class_id "
              "WHERE times.class_id != ?",
        SqliteParameters{SqliteValue{std::int64_t{classId}}}
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }

    for (const SqliteRow& row : rows->rows)
    {
        if (row.values.size() != 5)
        {
            return std::unexpected(error(
                ErrorCode::Schema,
                "SQLite returned an unexpected class conflict row shape."
                ));
        }
        const Result<int> conflictingId = integerValue(row.values[0], "class_id");
        const Result<std::string> conflictingName = textValue(row.values[1], "class_name");
        const Result<std::string> day = textValue(row.values[2], "day");
        const Result<std::string> start = textValue(row.values[3], "start_time");
        const Result<std::string> end = textValue(row.values[4], "end_time");
        if (!conflictingId)
        {
            return std::unexpected(conflictingId.error());
        }
        if (!conflictingName)
        {
            return std::unexpected(conflictingName.error());
        }
        if (!day)
        {
            return std::unexpected(day.error());
        }
        if (!start)
        {
            return std::unexpected(start.error());
        }
        if (!end)
        {
            return std::unexpected(end.error());
        }

        const ClassTime existing{*day, *start, *end};
        const std::optional<TimeInterval> existingInterval = toInterval(existing);
        if (!existingInterval)
        {
            continue;
        }
        const std::string otherName = classDisplayName(
            *conflictingName,
            *conflictingId
            );
        for (std::size_t index = 0; index < times.size(); ++index)
        {
            if (candidateIntervals[index]
                && intervalsOverlap(*candidateIntervals[index], *existingInterval))
            {
                conflicts.push_back({
                    classId,
                    currentName,
                    times[index].day,
                    times[index].startTime,
                    times[index].endTime,
                    otherName
                });
            }
        }
    }

    return conflicts;
}

} // namespace classmngr::engine
