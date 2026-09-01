#include "classmngr/engine/schedule_import_service.h"

#include "classmngr/engine/application_settings_service.h"
#include "classmngr/engine/class_info_config.h"
#include "classmngr/engine/class_repository.h"
#include "classmngr/engine/class_schedule_service.h"
#include "classmngr/engine/sqlite_database.h"
#include "classmngr/engine/teacher_service.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <map>
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
constexpr int MinutesPerDay = 24 * 60;

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

std::string collapseWhitespace(std::string_view value)
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

std::string normalized(std::string_view value)
{
    std::string result = collapseWhitespace(value);
    for (char& character : result)
    {
        if (character >= 'A' && character <= 'Z')
        {
            character = static_cast<char>(character - 'A' + 'a');
        }
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

bool equalsInsensitive(
    std::string_view left,
    std::string_view right
    )
{
    return normalized(left) == normalized(right);
}

bool contains(
    const std::vector<std::string>& values,
    std::string_view value
    )
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

struct DecodedUtf8
{
    std::vector<std::uint32_t> codePoints;
    bool valid = true;
};

DecodedUtf8 decodeUtf8(std::string_view value)
{
    DecodedUtf8 result;
    result.codePoints.reserve(value.size());

    const auto byteAt = [value](std::size_t index)
    {
        return static_cast<unsigned char>(value[index]);
    };

    for (std::size_t index = 0; index < value.size();)
    {
        const unsigned char first = byteAt(index);
        std::uint32_t codePoint = 0;
        std::size_t width = 0;
        std::uint32_t minimum = 0;

        if (first <= 0x7F)
        {
            codePoint = first;
            width = 1;
            minimum = 0;
        }
        else if (first >= 0xC2 && first <= 0xDF)
        {
            codePoint = first & 0x1F;
            width = 2;
            minimum = 0x80;
        }
        else if (first >= 0xE0 && first <= 0xEF)
        {
            codePoint = first & 0x0F;
            width = 3;
            minimum = 0x800;
        }
        else if (first >= 0xF0 && first <= 0xF4)
        {
            codePoint = first & 0x07;
            width = 4;
            minimum = 0x10000;
        }
        else
        {
            result.valid = false;
            ++index;
            continue;
        }

        if (index + width > value.size())
        {
            result.valid = false;
            ++index;
            continue;
        }

        bool continuationBytes = true;
        for (std::size_t offset = 1; offset < width; ++offset)
        {
            const unsigned char continuation = byteAt(index + offset);
            if ((continuation & 0xC0) != 0x80)
            {
                continuationBytes = false;
                break;
            }
            codePoint = (codePoint << 6) | (continuation & 0x3F);
        }

        if (!continuationBytes
            || codePoint < minimum
            || codePoint > 0x10FFFF
            || (codePoint >= 0xD800 && codePoint <= 0xDFFF))
        {
            result.valid = false;
            ++index;
            continue;
        }

        result.codePoints.push_back(codePoint);
        index += width;
    }

    return result;
}

bool isHangul(std::uint32_t codePoint) noexcept
{
    return (codePoint >= 0x1100 && codePoint <= 0x11FF)
        || (codePoint >= 0x3130 && codePoint <= 0x318F)
        || (codePoint >= 0xA960 && codePoint <= 0xA97F)
        || (codePoint >= 0xAC00 && codePoint <= 0xD7FF);
}

void appendUtf8(
    std::string& result,
    std::uint32_t codePoint
    )
{
    if (codePoint <= 0x7F)
    {
        result.push_back(static_cast<char>(codePoint));
    }
    else if (codePoint <= 0x7FF)
    {
        result.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
    else if (codePoint <= 0xFFFF)
    {
        result.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        result.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
    else
    {
        result.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        result.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

std::string hangulOnly(std::string_view value)
{
    const DecodedUtf8 decoded = decodeUtf8(value);
    std::string result;
    result.reserve(value.size());
    for (const std::uint32_t codePoint : decoded.codePoints)
    {
        if (isHangul(codePoint))
        {
            appendUtf8(result, codePoint);
        }
    }
    return result;
}

bool validKind(ScheduleImportKind kind) noexcept
{
    return kind == ScheduleImportKind::Normal
        || kind == ScheduleImportKind::Intensive;
}

bool validTeacherAction(ScheduleImportTeacherAction action) noexcept
{
    return action == ScheduleImportTeacherAction::Reuse
        || action == ScheduleImportTeacherAction::UpdateRoom
        || action == ScheduleImportTeacherAction::Create
        || action == ScheduleImportTeacherAction::Skip;
}

bool validClassAction(ScheduleImportClassAction action) noexcept
{
    return action == ScheduleImportClassAction::UpdateExisting
        || action == ScheduleImportClassAction::CreateNew
        || action == ScheduleImportClassAction::Skip;
}

bool validCourse(
    std::string_view grade,
    std::string_view level
    )
{
    if (!contains(ClassInfoConfig::grades(), grade))
    {
        return false;
    }
    return contains(ClassInfoConfig::levelsForGrade(grade), level);
}

std::optional<int> parseClock(std::string_view value)
{
    const std::string normalizedValue = trimAsciiWhitespace(value);
    const std::size_t colon = normalizedValue.find(':');
    if (colon == std::string::npos || colon == 0 || colon > 2)
    {
        return std::nullopt;
    }

    const auto digits = [&normalizedValue](std::size_t first, std::size_t last)
    {
        for (std::size_t index = first; index < last; ++index)
        {
            if (normalizedValue[index] < '0'
                || normalizedValue[index] > '9')
            {
                return false;
            }
        }
        return true;
    };

    if (normalizedValue.size() == colon + 3
        && digits(0, colon)
        && digits(colon + 1, colon + 3))
    {
        const int hour = colon == 1
            ? normalizedValue[0] - '0'
            : (normalizedValue[0] - '0') * 10
                + normalizedValue[1] - '0';
        const int minute = (normalizedValue[colon + 1] - '0') * 10
            + normalizedValue[colon + 2] - '0';
        if (hour <= 23 && minute <= 59)
        {
            return hour * 60 + minute;
        }
        return std::nullopt;
    }

    std::size_t period = 0;
    if (normalizedValue.size() == colon + 5)
    {
        period = colon + 3;
    }
    else if (normalizedValue.size() == colon + 6
             && normalizedValue[colon + 3] == ' ')
    {
        period = colon + 4;
    }
    else
    {
        return std::nullopt;
    }

    if (!digits(0, colon)
        || !digits(colon + 1, colon + 3)
        || (normalizedValue[period] != 'A'
            && normalizedValue[period] != 'a'
            && normalizedValue[period] != 'P'
            && normalizedValue[period] != 'p')
        || normalizedValue[period + 1] != 'M'
           && normalizedValue[period + 1] != 'm')
    {
        return std::nullopt;
    }
    if (colon == 2 && normalizedValue[0] == '0')
    {
        return std::nullopt;
    }

    const int hour = colon == 1
        ? normalizedValue[0] - '0'
        : (normalizedValue[0] - '0') * 10
            + normalizedValue[1] - '0';
    const int minute = (normalizedValue[colon + 1] - '0') * 10
        + normalizedValue[colon + 2] - '0';
    if (hour < 1 || hour > 12 || minute > 59)
    {
        return std::nullopt;
    }

    int hour24 = hour % 12;
    const char periodCharacter = normalizedValue[period];
    if (periodCharacter == 'P' || periodCharacter == 'p')
    {
        hour24 += 12;
    }
    return hour24 * 60 + minute;
}

std::optional<int> parseStrict24Hour(std::string_view value)
{
    const std::string normalizedValue = trimAsciiWhitespace(value);
    if (normalizedValue.size() != 5
        || normalizedValue[2] != ':'
        || normalizedValue[0] < '0'
        || normalizedValue[0] > '9'
        || normalizedValue[1] < '0'
        || normalizedValue[1] > '9'
        || normalizedValue[3] < '0'
        || normalizedValue[3] > '9'
        || normalizedValue[4] < '0'
        || normalizedValue[4] > '9')
    {
        return std::nullopt;
    }

    const int hour = (normalizedValue[0] - '0') * 10
        + normalizedValue[1] - '0';
    const int minute = (normalizedValue[3] - '0') * 10
        + normalizedValue[4] - '0';
    if (hour > 23 || minute > 59)
    {
        return std::nullopt;
    }
    return hour * 60 + minute;
}

int weekdayIndex(std::string_view value)
{
    const std::string day = normalized(value);
    for (std::size_t index = 0; index < ClassInfoConfig::days().size(); ++index)
    {
        if (normalized(ClassInfoConfig::days()[index]) == day)
        {
            return static_cast<int>(index);
        }
    }
    return -1;
}

std::optional<std::pair<int, int>> timeInterval(const ClassTime& time)
{
    const int day = weekdayIndex(time.day);
    const std::optional<int> start = parseClock(time.startTime);
    const std::optional<int> end = parseClock(time.endTime);
    if (day < 0 || !start || !end || *end <= *start)
    {
        return std::nullopt;
    }

    return std::pair<int, int>{
        day * MinutesPerDay + *start,
        day * MinutesPerDay + *end
    };
}

int dayGroup(const std::vector<ClassTime>& times)
{
    int group = 0;
    for (const ClassTime& time : times)
    {
        const std::string day = normalized(time.day);
        const int current = day == "monday"
                || day == "wednesday"
                || day == "friday"
            ? 1
            : day == "tuesday" || day == "thursday"
                ? 2
                : 0;
        if (current == 0 || (group != 0 && group != current))
        {
            return 0;
        }
        group = current;
    }
    return group;
}

bool daysAreCompatible(
    const std::vector<ClassTime>& imported,
    const std::vector<ClassTime>& existing
    )
{
    const int importedGroup = dayGroup(imported);
    return importedGroup != 0 && importedGroup == dayGroup(existing);
}

std::vector<std::string> meetingDays(const std::vector<ClassTime>& times)
{
    std::vector<std::string> days;
    for (const ClassTime& time : times)
    {
        const std::string day = normalized(time.day);
        if (std::find(days.begin(), days.end(), day) == days.end())
        {
            days.push_back(day);
        }
    }
    std::sort(days.begin(), days.end());
    return days;
}

bool meetingDaysMatch(
    const std::vector<ClassTime>& imported,
    const std::vector<ClassTime>& existing
    )
{
    return daysAreCompatible(imported, existing)
        && meetingDays(imported) == meetingDays(existing);
}

std::vector<ClassTime> timesForKind(
    const ClassInfo& info,
    ScheduleImportKind kind
    )
{
    const std::vector<ClassTime>& preferred =
        kind == ScheduleImportKind::Intensive
            ? info.intensiveTimes
            : info.classTimes;
    if (!preferred.empty())
    {
        return preferred;
    }
    return kind == ScheduleImportKind::Intensive
        ? info.classTimes
        : info.intensiveTimes;
}

const std::vector<ClassTime>& targetTimesForKind(
    const ClassInfo& info,
    ScheduleImportKind kind
    )
{
    return kind == ScheduleImportKind::Intensive
        ? info.intensiveTimes
        : info.classTimes;
}

bool classOptionIsEligible(
    const ScheduleImportClassCandidate& candidate,
    const ClassInfo& existing,
    ScheduleImportKind kind
    )
{
    return equalsInsensitive(candidate.classGrade, existing.classGrade)
        && equalsInsensitive(candidate.classLevel, existing.classLevel)
        && (
            timesForKind(existing, kind).empty()
            || daysAreCompatible(
                candidate.times,
                timesForKind(existing, kind)
                )
            );
}

std::vector<std::vector<std::string>> allowedDayPatterns(
    std::string_view classGrade,
    std::string_view classLevel
    )
{
    const std::string grade = upperAscii(trimAsciiWhitespace(classGrade));
    const std::string level = trimAsciiWhitespace(classLevel);
    const bool songs = equalsInsensitive(level, "Song's");

    const std::vector<std::string> mondayWednesday{
        "Monday", "Wednesday"
    };
    const std::vector<std::string> mondayFriday{
        "Monday", "Friday"
    };
    const std::vector<std::string> wednesdayFriday{
        "Wednesday", "Friday"
    };
    const std::vector<std::string> tuesdayThursday{
        "Tuesday", "Thursday"
    };
    const std::vector<std::string> mondayWednesdayFriday{
        "Monday", "Wednesday", "Friday"
    };

    if (grade == "E4"
        || (grade == "E5" && !equalsInsensitive(level, "Athena")))
    {
        return {
            mondayWednesday,
            mondayFriday,
            wednesdayFriday,
            tuesdayThursday
        };
    }
    if (grade == "E5" && equalsInsensitive(level, "Athena"))
    {
        return {mondayWednesdayFriday, tuesdayThursday};
    }
    if (grade == "E6" && songs)
    {
        return {mondayWednesdayFriday, tuesdayThursday};
    }
    if ((grade == "M1" || grade == "M2" || grade == "M3") && songs)
    {
        return {
            mondayWednesday,
            mondayFriday,
            wednesdayFriday,
            tuesdayThursday
        };
    }
    if (grade == "E6" || grade == "M1" || grade == "M2")
    {
        return {
            {"Monday"},
            {"Tuesday"},
            {"Wednesday"},
            {"Thursday"},
            {"Friday"}
        };
    }
    return {};
}

std::string patternKey(std::vector<std::string> days)
{
    std::sort(
        days.begin(),
        days.end(),
        [](const std::string& left, const std::string& right)
        {
            return weekdayIndex(left) < weekdayIndex(right);
        }
        );

    std::string result;
    for (const std::string& day : days)
    {
        if (!result.empty())
        {
            result += '|';
        }
        result += day;
    }
    return result;
}

std::string meetingPatternError(
    const ScheduleImportClassCandidate& candidate
    )
{
    std::vector<std::string> days;
    for (const ClassTime& time : candidate.times)
    {
        const int day = weekdayIndex(time.day);
        if (day < 0 || day > 4
            || std::find(days.begin(), days.end(),
                ClassInfoConfig::days()[static_cast<std::size_t>(day)])
                != days.end())
        {
            return "Each imported class must have exactly one meeting per "
                "scheduled weekday.";
        }
        days.push_back(ClassInfoConfig::days()[static_cast<std::size_t>(day)]);
    }

    const auto allowed = allowedDayPatterns(
        candidate.classGrade,
        candidate.classLevel
        );
    if (allowed.empty())
    {
        return {};
    }

    const std::string key = patternKey(days);
    for (const std::vector<std::string>& pattern : allowed)
    {
        if (patternKey(pattern) == key)
        {
            return {};
        }
    }

    std::string expectation;
    const std::string grade = upperAscii(
        trimAsciiWhitespace(candidate.classGrade)
        );
    const std::string level = trimAsciiWhitespace(candidate.classLevel);
    const bool songs = equalsInsensitive(level, "Song's");
    if (grade == "E4"
        || (grade == "E5" && !equalsInsensitive(level, "Athena"))
        || ((grade == "M1" || grade == "M2" || grade == "M3") && songs))
    {
        expectation =
            "Expected Monday/Wednesday, Monday/Friday, Wednesday/Friday, "
            "or Tuesday/Thursday.";
    }
    else if ((grade == "E5" && equalsInsensitive(level, "Athena"))
             || (grade == "E6" && songs))
    {
        expectation =
            "Expected Monday/Wednesday/Friday or Tuesday/Thursday.";
    }
    else if (grade == "E6" || grade == "M1" || grade == "M2")
    {
        expectation = "Expected one weekday meeting.";
    }
    else
    {
        expectation =
            "The imported grade and level do not have a supported "
            "meeting-pattern rule.";
    }

    std::string detected;
    for (const std::string& day : days)
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
    return expectation + " Detected: " + detected + '.';
}

std::string normalizedHexColor(std::string_view value)
{
    const std::string color = trimAsciiWhitespace(value);
    if (color.size() != 7 || color[0] != '#')
    {
        return {};
    }
    for (std::size_t index = 1; index < color.size(); ++index)
    {
        const char character = color[index];
        const bool digit = character >= '0' && character <= '9';
        const bool lowerHex = character >= 'a' && character <= 'f';
        const bool upperHex = character >= 'A' && character <= 'F';
        if (!digit && !lowerHex && !upperHex)
        {
            return {};
        }
    }

    std::string result = color;
    for (char& character : result)
    {
        if (character >= 'a' && character <= 'f')
        {
            character = static_cast<char>(character - 'a' + 'A');
        }
    }
    return result;
}

std::string courseLabel(
    std::string_view grade,
    std::string_view level
    )
{
    std::string result = trimAsciiWhitespace(grade);
    const std::string trimmedLevel = trimAsciiWhitespace(level);
    if (!trimmedLevel.empty())
    {
        if (!result.empty())
        {
            result.push_back(' ');
        }
        result += trimmedLevel;
    }
    return result;
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
            "SQLite returned an invalid " + std::string(column) + " value."
            ));
    }
    return static_cast<int>(*integer);
}

struct ExistingState
{
    std::vector<Teacher> teachers;
    std::vector<Classroom> classes;
    std::map<int, ClassInfo> info;
};

Result<ExistingState> loadExisting(SqliteDatabase& database)
{
    TeacherService teacherService(database);
    ClassRepository classRepository(database);
    ClassScheduleService scheduleService(database);

    const Result<std::vector<Teacher>> teachers = teacherService.list();
    if (!teachers)
    {
        return std::unexpected(teachers.error());
    }
    const Result<std::vector<Classroom>> classes = classRepository.list();
    if (!classes)
    {
        return std::unexpected(classes.error());
    }
    const Result<std::vector<ClassInfo>> infos =
        scheduleService.loadScheduleClassInfos();
    if (!infos)
    {
        return std::unexpected(infos.error());
    }

    ExistingState result;
    result.teachers = *teachers;
    result.classes = *classes;
    for (const ClassInfo& info : *infos)
    {
        if (!result.info.emplace(info.classId, info).second)
        {
            return std::unexpected(error(
                ErrorCode::Schema,
                "SQLite returned duplicate class information for id "
                    + std::to_string(info.classId) + "."
                ));
        }
    }
    for (const Classroom& classroom : result.classes)
    {
        if (!result.info.contains(classroom.id))
        {
            return std::unexpected(error(
                ErrorCode::Schema,
                "SQLite did not return class information for class id "
                    + std::to_string(classroom.id) + "."
                ));
        }
    }
    return result;
}

struct ValidatedPlan
{
    std::map<std::string, ScheduleImportTeacherResolution> teachers;
    std::map<int, ScheduleImportClassResolution> classes;
};

Result<ValidatedPlan> validatePlan(const ScheduleImportPlan& plan)
{
    if (!validKind(plan.kind))
    {
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            "The schedule import has an unsupported schedule kind."
            ));
    }
    if (plan.kind == ScheduleImportKind::Intensive
        && plan.intensiveMode != ScheduleImportIntensiveMode::UpdateExisting
        && plan.intensiveMode != ScheduleImportIntensiveMode::ReplaceWithNew)
    {
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            "Choose how the existing intensive schedule should be handled."
            ));
    }
    if (!plan.diagnostics.empty() && !plan.unknownCellsAcknowledged)
    {
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            "Unrecognized timetable cells must be acknowledged before "
            "importing."
            ));
    }

    ValidatedPlan result;
    for (const ScheduleImportTeacherResolution& resolution : plan.teachers)
    {
        if (!validTeacherAction(resolution.action)
            || trimAsciiWhitespace(resolution.teacherKey).empty()
            || result.teachers.contains(resolution.teacherKey))
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "The teacher import plan contains an invalid or duplicate "
                "resolution."
                ));
        }
        result.teachers.emplace(resolution.teacherKey, resolution);
    }

    std::set<int> claimedTargets;
    for (const ScheduleImportClassResolution& resolution : plan.classes)
    {
        if (!validClassAction(resolution.action)
            || resolution.candidateIndex < 0
            || resolution.candidateIndex
                >= static_cast<int>(plan.candidates.size())
            || result.classes.contains(resolution.candidateIndex))
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "The class import plan contains an invalid or duplicate "
                "resolution."
                ));
        }

        if (resolution.action == ScheduleImportClassAction::UpdateExisting
            && (resolution.targetClassId <= 0
                || claimedTargets.contains(resolution.targetClassId)))
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "Each updated class must have a unique existing target."
                ));
        }
        if (resolution.targetClassId > 0
            && (resolution.action == ScheduleImportClassAction::UpdateExisting
                || resolution.action == ScheduleImportClassAction::Skip))
        {
            if (claimedTargets.contains(resolution.targetClassId))
            {
                return std::unexpected(error(
                    ErrorCode::InvalidFormat,
                    "Each imported class must resolve to a unique existing "
                    "target."
                    ));
            }
            claimedTargets.insert(resolution.targetClassId);
        }
        if (resolution.action == ScheduleImportClassAction::CreateNew
            && resolution.targetClassId > 0)
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "A newly created class cannot have an existing target."
                ));
        }
        result.classes.emplace(resolution.candidateIndex, resolution);
    }

    std::set<std::string> candidateTeacherKeys;
    std::map<std::string, std::vector<std::string>> importedRooms;
    for (const ScheduleImportClassCandidate& candidate : plan.candidates)
    {
        if (!validCourse(candidate.classGrade, candidate.classLevel)
            || trimAsciiWhitespace(candidate.teacherKey).empty()
            || hangulOnly(candidate.teacherKr) != candidate.teacherKey
            || candidate.times.empty())
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "The import contains an invalid class."
                ));
        }

        candidateTeacherKeys.insert(candidate.teacherKey);
        for (const std::string& room : candidate.rooms)
        {
            const std::string trimmedRoom = trimAsciiWhitespace(room);
            if (!trimmedRoom.empty()
                && std::find(
                    importedRooms[candidate.teacherKey].begin(),
                    importedRooms[candidate.teacherKey].end(),
                    trimmedRoom
                    ) == importedRooms[candidate.teacherKey].end())
            {
                importedRooms[candidate.teacherKey].push_back(trimmedRoom);
            }
        }
    }

    if (result.teachers.size() != candidateTeacherKeys.size()
        || result.classes.size() != plan.candidates.size())
    {
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            "Every imported teacher and class requires a resolution."
            ));
    }

    for (const std::string& key : candidateTeacherKeys)
    {
        const auto teacherResolution = result.teachers.find(key);
        if (teacherResolution == result.teachers.end())
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "Every imported teacher requires a matching resolution."
                ));
        }

        const ScheduleImportTeacherResolution& resolution =
            teacherResolution->second;
        const std::string room = trimAsciiWhitespace(resolution.selectedRoom);
        const bool roomMustBeSelected =
            resolution.action == ScheduleImportTeacherAction::Create
            || resolution.action == ScheduleImportTeacherAction::UpdateRoom
            || (resolution.action != ScheduleImportTeacherAction::Skip
                && importedRooms[key].size() > 1);
        if (roomMustBeSelected
            && (room.empty()
                || std::find(
                    importedRooms[key].begin(),
                    importedRooms[key].end(),
                    room
                    ) == importedRooms[key].end()))
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "Choose one of the imported rooms for every unresolved "
                "Korean teacher."
                ));
        }
    }

    for (std::size_t index = 0; index < plan.candidates.size(); ++index)
    {
        const ScheduleImportClassCandidate& candidate = plan.candidates[index];
        const ScheduleImportTeacherResolution& teacherResolution =
            result.teachers.at(candidate.teacherKey);
        const ScheduleImportClassResolution& classResolution =
            result.classes.at(static_cast<int>(index));

        if (teacherResolution.action == ScheduleImportTeacherAction::Skip
            && classResolution.action != ScheduleImportClassAction::Skip)
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "Classes assigned to a skipped Korean teacher must also be "
                "skipped."
                ));
        }

        if (classResolution.action != ScheduleImportClassAction::Skip)
        {
            const std::string patternError = meetingPatternError(candidate);
            if (!patternError.empty())
            {
                return std::unexpected(error(
                    ErrorCode::InvalidFormat,
                    "The meeting pattern for "
                        + courseLabel(candidate.classGrade, candidate.classLevel)
                        + " is invalid: " + patternError
                    ));
            }
            if (normalizedHexColor(classResolution.classColor).empty()
                || normalizedHexColor(classResolution.fontColor).empty())
            {
                return std::unexpected(error(
                    ErrorCode::InvalidFormat,
                    "Choose a valid class color for every imported class."
                    ));
            }
        }
    }

    return result;
}

Status validateIntensiveSlotStates(
    const std::vector<IntensiveSlotState>& states
    )
{
    static const std::set<std::string> validStates{
        "empty", "essay", "lunch"
    };
    std::set<std::string> keys;
    for (const IntensiveSlotState& state : states)
    {
        if (weekdayIndex(state.day) < 0
            || !parseStrict24Hour(state.startTime)
            || validStates.find(state.state) == validStates.end())
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "The import contains an invalid intensive slot state."
                ));
        }
        const std::string key = state.day + '\x1f' + state.startTime;
        if (!keys.insert(key).second)
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "The import contains duplicate intensive slot states."
                ));
        }
    }
    return {};
}

Status validateProjectedSchedule(
    SqliteDatabase& database,
    const ScheduleImportPlan& plan,
    const std::map<int, ClassInfo>& existingInfo,
    const std::map<int, ScheduleImportClassResolution>& resolutions
    )
{
    struct ProjectedClass
    {
        std::string label;
        std::vector<ClassTime> times;
    };

    const bool preservesAbsent =
        plan.kind == ScheduleImportKind::Intensive
        && plan.intensiveMode == ScheduleImportIntensiveMode::UpdateExisting;
    std::map<int, ProjectedClass> projectedClasses;
    if (preservesAbsent)
    {
        for (const auto& [classId, info] : existingInfo)
        {
            const std::vector<ClassTime> times = timesForKind(info, plan.kind);
            if (!times.empty())
            {
                projectedClasses.emplace(
                    classId,
                    ProjectedClass{
                        courseLabel(info.classGrade, info.classLevel),
                        times
                    }
                    );
            }
        }
    }

    for (std::size_t index = 0; index < plan.candidates.size(); ++index)
    {
        const ScheduleImportClassResolution& resolution = resolutions.at(
            static_cast<int>(index)
            );
        const ScheduleImportClassCandidate& candidate = plan.candidates[index];
        if (resolution.action == ScheduleImportClassAction::Skip)
        {
            if (!preservesAbsent
                && resolution.targetClassId > 0)
            {
                const auto found = existingInfo.find(resolution.targetClassId);
                if (found != existingInfo.end())
                {
                    projectedClasses[resolution.targetClassId] = {
                        courseLabel(
                            found->second.classGrade,
                            found->second.classLevel
                            ),
                        timesForKind(found->second, plan.kind)
                    };
                }
            }
            continue;
        }

        const int projectedId =
            resolution.action == ScheduleImportClassAction::UpdateExisting
                ? resolution.targetClassId
                : -static_cast<int>(index + 1);
        projectedClasses[projectedId] = {
            courseLabel(candidate.classGrade, candidate.classLevel),
            candidate.times
        };
    }

    std::vector<ClassScheduleEntry> projected;
    for (const auto& [classId, classroom] : projectedClasses)
    {
        for (const ClassTime& time : classroom.times)
        {
            if (!timeInterval(time))
            {
                return std::unexpected(error(
                    ErrorCode::InvalidFormat,
                    classroom.label + " contains an invalid time: "
                        + time.day + ' ' + time.startTime + " - "
                        + time.endTime
                    ));
            }
            projected.push_back({classId, classroom.label, time});
        }
    }

    ClassScheduleService scheduleService(database);
    const std::vector<ClassConflict> conflicts =
        scheduleService.findConflicts(projected, {});
    if (!conflicts.empty())
    {
        const ClassConflict& conflict = conflicts.front();
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            "The proposed schedule overlaps: " + conflict.className
                + " conflicts with " + conflict.conflictingClassName
                + " on " + conflict.day + "."
            ));
    }

    return {};
}

Status validateCurrentState(
    SqliteDatabase& database,
    const ScheduleImportPlan& plan,
    const ValidatedPlan& validated,
    const ExistingState& existing
    )
{
    std::map<int, const Teacher*> teachersById;
    for (const Teacher& teacher : existing.teachers)
    {
        teachersById.emplace(teacher.id, &teacher);
    }
    std::set<int> classIds;
    for (const Classroom& classroom : existing.classes)
    {
        classIds.insert(classroom.id);
    }

    for (const auto& [key, resolution] : validated.teachers)
    {
        if ((resolution.action == ScheduleImportTeacherAction::Reuse
             || resolution.action == ScheduleImportTeacherAction::UpdateRoom)
            && (resolution.targetTeacherId <= 0
                || !teachersById.contains(resolution.targetTeacherId)
                || hangulOnly(
                    teachersById.at(resolution.targetTeacherId)->teacherKr
                    ) != key))
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "A selected Korean teacher is no longer available."
                ));
        }
        if ((resolution.action == ScheduleImportTeacherAction::Create
             || resolution.action == ScheduleImportTeacherAction::Skip)
            && resolution.targetTeacherId > 0)
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "A created or skipped Korean teacher cannot have an existing "
                "target."
                ));
        }
        if (resolution.action == ScheduleImportTeacherAction::UpdateRoom
            && trimAsciiWhitespace(resolution.selectedRoom).empty())
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "Choose a room before updating a Korean teacher."
                ));
        }
    }

    for (const auto& [index, resolution] : validated.classes)
    {
        if (resolution.action == ScheduleImportClassAction::UpdateExisting
            && !classIds.contains(resolution.targetClassId))
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "A selected class is no longer available."
                ));
        }
        if (resolution.action != ScheduleImportClassAction::Skip
            || resolution.targetClassId <= 0)
        {
            continue;
        }

        const ScheduleImportClassCandidate& candidate = plan.candidates.at(
            static_cast<std::size_t>(index)
            );
        std::vector<int> exactTargets;
        for (const Classroom& classroom : existing.classes)
        {
            const auto found = existing.info.find(classroom.id);
            if (found == existing.info.end())
            {
                continue;
            }
            const ClassInfo& info = found->second;
            const auto teacher = teachersById.find(info.teacherId);
            if (equalsInsensitive(info.classGrade, candidate.classGrade)
                && equalsInsensitive(info.classLevel, candidate.classLevel)
                && teacher != teachersById.end()
                && hangulOnly(teacher->second->teacherKr)
                    == candidate.teacherKey)
            {
                exactTargets.push_back(classroom.id);
            }
        }
        if (exactTargets.size() != 1
            || exactTargets.front() != resolution.targetClassId)
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "A skipped imported class can preserve only its unique "
                "exact existing match."
                ));
        }
    }

    if (plan.kind == ScheduleImportKind::Intensive)
    {
        const Status states = validateIntensiveSlotStates(
            plan.intensiveSlotStates
            );
        if (!states)
        {
            return states;
        }
    }

    return validateProjectedSchedule(
        database,
        plan,
        existing.info,
        validated.classes
        );
}

Status writeTimes(
    SqliteDatabase& database,
    std::string_view table,
    int classId,
    const std::vector<ClassTime>& times
    )
{
    for (const ClassTime& time : times)
    {
        const Status inserted = database.execute(
            std::string("INSERT INTO ") + std::string(table)
                + " (class_id, day, start_time, end_time) VALUES (?, ?, ?, ?)",
            SqliteParameters{
                SqliteValue{std::int64_t{classId}},
                SqliteValue{time.day},
                SqliteValue{time.startTime},
                SqliteValue{time.endTime}
            }
            );
        if (!inserted)
        {
            return inserted;
        }
    }
    return {};
}

Status writeIntensiveSlotStates(
    SqliteDatabase& database,
    const std::vector<IntensiveSlotState>& states
    )
{
    for (const IntensiveSlotState& state : states)
    {
        const Status inserted = database.execute(
            "INSERT INTO intensive_slot_states (day, start_time, state) "
            "VALUES (?, ?, ?)",
            SqliteParameters{
                SqliteValue{state.day},
                SqliteValue{state.startTime},
                SqliteValue{state.state}
            }
            );
        if (!inserted)
        {
            return inserted;
        }
    }
    return {};
}

Result<int> lastInsertId(
    SqliteDatabase& database,
    std::string_view entity
    )
{
    const auto row = database.query("SELECT last_insert_rowid()");
    if (!row)
    {
        return std::unexpected(row.error());
    }
    if (row->rows.size() != 1 || row->rows.front().values.size() != 1)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite did not return the new " + std::string(entity) + " id."
            ));
    }
    const Result<int> id = integerValue(
        row->rows.front().values.front(),
        std::string(entity) + "_id"
        );
    if (!id)
    {
        return std::unexpected(id.error());
    }
    if (*id <= 0)
    {
        return std::unexpected(error(
            ErrorCode::Database,
            "The imported " + std::string(entity)
                + " did not receive a valid id."
            ));
    }
    return id;
}

} // namespace

ScheduleImportService::ScheduleImportService(SqliteDatabase& database)
    : m_database(database)
{
}

Result<ScheduleImportPreview> ScheduleImportService::previewImport(
    const ScheduleImportUserBlock& user,
    ScheduleImportKind kind
    )
{
    if (!m_database.isOpen())
    {
        return std::unexpected(error(
            ErrorCode::Database,
            "SQLite database is not open."
            ));
    }
    if (!validKind(kind))
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "Schedule import preview requires a supported schedule kind."
            ));
    }

    const Result<ExistingState> existing = loadExisting(m_database);
    if (!existing)
    {
        return std::unexpected(existing.error());
    }

    ScheduleImportPreview result;
    result.kind = kind;
    result.user = user;
    result.inventory.classCount = static_cast<int>(existing->classes.size());
    for (const auto& [classId, info] : existing->info)
    {
        (void)classId;
        result.inventory.hasRegularHours =
            result.inventory.hasRegularHours || !info.classTimes.empty();
        result.inventory.hasIntensiveHours =
            result.inventory.hasIntensiveHours || !info.intensiveTimes.empty();
    }

    std::set<std::string> seenTeacherKeys;
    for (const ScheduleImportClassCandidate& candidate : user.classes)
    {
        if (!seenTeacherKeys.insert(candidate.teacherKey).second)
        {
            continue;
        }

        ScheduleImportTeacherPreview teacherPreview;
        teacherPreview.teacherKey = candidate.teacherKey;
        teacherPreview.teacherKr = candidate.teacherKr;
        for (const ScheduleImportClassCandidate& other : user.classes)
        {
            if (other.teacherKey != candidate.teacherKey)
            {
                continue;
            }
            for (const std::string& room : other.rooms)
            {
                const std::string normalizedRoom = trimAsciiWhitespace(room);
                if (!normalizedRoom.empty()
                    && std::find(
                        teacherPreview.importedRooms.begin(),
                        teacherPreview.importedRooms.end(),
                        normalizedRoom
                        ) == teacherPreview.importedRooms.end())
                {
                    teacherPreview.importedRooms.push_back(normalizedRoom);
                }
            }
        }
        for (const Teacher& teacher : existing->teachers)
        {
            if (hangulOnly(teacher.teacherKr) == candidate.teacherKey)
            {
                teacherPreview.matchingTeacherIds.push_back(teacher.id);
            }
        }
        for (const auto& [classId, info] : existing->info)
        {
            (void)classId;
            if (std::find(
                    teacherPreview.matchingTeacherIds.begin(),
                    teacherPreview.matchingTeacherIds.end(),
                    info.teacherId
                    ) != teacherPreview.matchingTeacherIds.end())
            {
                ++teacherPreview.affectedClassCount;
            }
        }
        result.teachers.push_back(std::move(teacherPreview));
    }

    std::set<int> exactTargets;
    for (std::size_t index = 0; index < user.classes.size(); ++index)
    {
        const ScheduleImportClassCandidate& candidate = user.classes[index];
        ScheduleImportClassPreview classPreview;
        classPreview.candidateIndex = static_cast<int>(index);

        std::vector<int> importedTeacherIds;
        for (const ScheduleImportTeacherPreview& teacher : result.teachers)
        {
            if (teacher.teacherKey == candidate.teacherKey)
            {
                importedTeacherIds = teacher.matchingTeacherIds;
                break;
            }
        }

        std::vector<int> exact;
        std::vector<int> sameCourseTeacherRoomSameDays;
        std::vector<int> sameCourseTeacherRoom;
        std::vector<int> sameCourseTeacherSameDays;
        std::vector<int> sameCourseTeacher;
        std::vector<int> sameCourseSameDays;
        std::vector<int> sameCourse;

        for (const Classroom& classroom : existing->classes)
        {
            const ClassInfo& info = existing->info.at(classroom.id);
            if (!classOptionIsEligible(candidate, info, kind))
            {
                continue;
            }

            const bool teacherMatches = std::find(
                importedTeacherIds.begin(),
                importedTeacherIds.end(),
                info.teacherId
                ) != importedTeacherIds.end();
            const bool roomMatches = std::any_of(
                candidate.rooms.begin(),
                candidate.rooms.end(),
                [&info](const std::string& room)
                {
                    return normalized(room) == normalized(info.roomNumber);
                }
                );
            const std::vector<ClassTime>& targetTimes = targetTimesForKind(
                info,
                kind
                );
            const bool targetDaysMatch = !targetTimes.empty()
                && meetingDaysMatch(candidate.times, targetTimes);
            const std::vector<ClassTime> referenceTimes = timesForKind(
                info,
                kind
                );
            const bool referenceDaysMatch = !referenceTimes.empty()
                && meetingDaysMatch(candidate.times, referenceTimes);

            if (teacherMatches && roomMatches && targetDaysMatch)
            {
                exact.push_back(classroom.id);
            }
            else if (teacherMatches && roomMatches && referenceDaysMatch)
            {
                sameCourseTeacherRoomSameDays.push_back(classroom.id);
            }
            else if (teacherMatches && roomMatches)
            {
                sameCourseTeacherRoom.push_back(classroom.id);
            }
            else if (teacherMatches && referenceDaysMatch)
            {
                sameCourseTeacherSameDays.push_back(classroom.id);
            }
            else if (teacherMatches)
            {
                sameCourseTeacher.push_back(classroom.id);
            }
            else if (referenceDaysMatch)
            {
                sameCourseSameDays.push_back(classroom.id);
            }
            else
            {
                sameCourse.push_back(classroom.id);
            }
        }

        for (const std::vector<int>& matches : {
                 exact,
                 sameCourseTeacherRoomSameDays,
                 sameCourseTeacherRoom,
                 sameCourseTeacherSameDays,
                 sameCourseTeacher,
                 sameCourseSameDays,
                 sameCourse
             })
        {
            for (const int classId : matches)
            {
                if (std::find(
                        classPreview.matchingClassIds.begin(),
                        classPreview.matchingClassIds.end(),
                        classId
                        ) == classPreview.matchingClassIds.end())
                {
                    classPreview.matchingClassIds.push_back(classId);
                }
            }
        }

        if (exact.size() == 1)
        {
            classPreview.suggestedClassId = exact.front();
            classPreview.exactMatch = true;
            classPreview.matchConfidence =
                ScheduleImportClassMatchConfidence::Confident;
            classPreview.matchExplanation =
                "One existing class matches the imported grade, level, "
                "Korean teacher, room, and meeting days.";
            exactTargets.insert(exact.front());
        }
        else if (!classPreview.matchingClassIds.empty())
        {
            classPreview.suggestedClassId =
                classPreview.matchingClassIds.front();
            classPreview.matchConfidence =
                ScheduleImportClassMatchConfidence::Possible;
            bool hasTargetHours = false;
            bool hasOtherHours = false;
            for (const int classId : classPreview.matchingClassIds)
            {
                const ClassInfo& info = existing->info.at(classId);
                hasTargetHours = hasTargetHours
                    || !targetTimesForKind(info, kind).empty();
                hasOtherHours = hasOtherHours
                    || !timesForKind(info, kind).empty();
            }
            classPreview.matchExplanation = hasTargetHours
                ? "Possible existing classes share the imported grade and "
                    "level and have a compatible weekday group."
                : hasOtherHours
                    ? "Possible existing classes have hours only in the "
                        "other schedule type; their grade, level, and "
                        "weekday group are compatible."
                    : "Possible existing classes share the imported grade "
                        "and level but have no schedule hours to compare.";
        }
        else
        {
            classPreview.matchExplanation =
                "No existing class has the same grade and level with a "
                "compatible weekday group.";
        }
        result.classes.push_back(std::move(classPreview));
    }

    for (const Classroom& classroom : existing->classes)
    {
        if (!exactTargets.contains(classroom.id))
        {
            result.initiallyAbsentClassIds.push_back(classroom.id);
        }
    }
    return result;
}

Result<ScheduleImportSummary> ScheduleImportService::importSchedule(
    const ScheduleImportPlan& plan
    )
{
    if (!m_database.isOpen())
    {
        return std::unexpected(error(
            ErrorCode::Database,
            "SQLite database is not open."
            ));
    }

    const Result<ValidatedPlan> validated = validatePlan(plan);
    if (!validated)
    {
        return std::unexpected(validated.error());
    }

    Result<SqliteTransaction> transactionResult =
        m_database.beginTransaction();
    if (!transactionResult)
    {
        return std::unexpected(transactionResult.error());
    }
    SqliteTransaction transaction = std::move(*transactionResult);

    const Result<ExistingState> existing = loadExisting(m_database);
    if (!existing)
    {
        return std::unexpected(existing.error());
    }
    const Status currentState = validateCurrentState(
        m_database,
        plan,
        *validated,
        *existing
        );
    if (!currentState)
    {
        return std::unexpected(currentState.error());
    }

    ScheduleImportSummary summary;
    summary.ignoredCells = static_cast<int>(plan.diagnostics.size());
    std::map<std::string, int> resolvedTeacherIds;
    TeacherService teacherService(m_database);

    for (const auto& [key, resolution] : validated->teachers)
    {
        if (resolution.action == ScheduleImportTeacherAction::Skip)
        {
            resolvedTeacherIds.emplace(key, -1);
            continue;
        }

        if (resolution.action == ScheduleImportTeacherAction::Create)
        {
            Teacher imported;
            for (const ScheduleImportClassCandidate& candidate : plan.candidates)
            {
                if (candidate.teacherKey == key)
                {
                    imported.teacherKr = hangulOnly(candidate.teacherKr);
                    break;
                }
            }
            imported.roomNumber = trimAsciiWhitespace(resolution.selectedRoom);
            const Result<int> teacherId = teacherService.create(imported);
            if (!teacherId)
            {
                return std::unexpected(teacherId.error());
            }
            resolvedTeacherIds.emplace(key, *teacherId);
            ++summary.teachersCreated;
            continue;
        }

        resolvedTeacherIds.emplace(key, resolution.targetTeacherId);
        if (resolution.action == ScheduleImportTeacherAction::UpdateRoom)
        {
            const Status updated = m_database.execute(
                "UPDATE teachers SET room_number=? WHERE id=?",
                SqliteParameters{
                    SqliteValue{
                        trimAsciiWhitespace(resolution.selectedRoom)
                    },
                    SqliteValue{
                        std::int64_t{resolution.targetTeacherId}
                    }
                }
                );
            if (!updated)
            {
                return std::unexpected(updated.error());
            }
            ++summary.teachersUpdated;
        }
    }

    const bool preservesAbsentIntensiveClasses =
        plan.kind == ScheduleImportKind::Intensive
        && plan.intensiveMode == ScheduleImportIntensiveMode::UpdateExisting;
    const std::string timeTable = plan.kind == ScheduleImportKind::Intensive
        ? "class_intensive_times"
        : "class_times";
    std::map<int, std::vector<ClassTime>> finalTimes;
    ClassRepository classRepository(m_database);

    for (std::size_t index = 0; index < plan.candidates.size(); ++index)
    {
        const ScheduleImportClassCandidate& candidate = plan.candidates[index];
        const ScheduleImportClassResolution& resolution = validated->classes.at(
            static_cast<int>(index)
            );

        if (resolution.action == ScheduleImportClassAction::Skip)
        {
            ++summary.classesSkipped;
            if (!preservesAbsentIntensiveClasses
                && resolution.targetClassId > 0)
            {
                const auto found = existing->info.find(resolution.targetClassId);
                if (found != existing->info.end())
                {
                    finalTimes[resolution.targetClassId] = timesForKind(
                        found->second,
                        plan.kind
                        );
                }
            }
            continue;
        }

        const auto teacherId = resolvedTeacherIds.find(candidate.teacherKey);
        if (teacherId == resolvedTeacherIds.end() || teacherId->second <= 0)
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "A class cannot be imported because its Korean teacher was "
                "skipped."
                ));
        }

        int classId = resolution.targetClassId;
        if (resolution.action == ScheduleImportClassAction::CreateNew)
        {
            const Result<int> created = classRepository.create(courseLabel(
                candidate.classGrade,
                candidate.classLevel
                ));
            if (!created)
            {
                return std::unexpected(created.error());
            }
            classId = *created;
            ++summary.classesCreated;
        }
        else
        {
            ++summary.classesUpdated;
        }

        const std::string classColor = normalizedHexColor(
            resolution.classColor
            );
        const std::string fontColor = normalizedHexColor(
            resolution.fontColor
            );
        const Status classInfoWritten = m_database.execute(
            "INSERT INTO class_info ("
            "class_id, teacher_id, class_grade, class_level, class_color, "
            "font_color"
            ") VALUES (?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(class_id) DO UPDATE SET "
            "teacher_id=excluded.teacher_id, "
            "class_grade=excluded.class_grade, "
            "class_level=excluded.class_level, "
            "class_color=excluded.class_color, "
            "font_color=excluded.font_color",
            SqliteParameters{
                SqliteValue{std::int64_t{classId}},
                SqliteValue{std::int64_t{teacherId->second}},
                SqliteValue{candidate.classGrade},
                SqliteValue{candidate.classLevel},
                SqliteValue{classColor},
                SqliteValue{fontColor}
            }
            );
        if (!classInfoWritten)
        {
            return std::unexpected(classInfoWritten.error());
        }
        finalTimes[classId] = candidate.times;
    }

    if (!preservesAbsentIntensiveClasses)
    {
        for (const Classroom& classroom : existing->classes)
        {
            const ClassInfo& info = existing->info.at(classroom.id);
            if (!timesForKind(info, plan.kind).empty()
                && !finalTimes.contains(classroom.id))
            {
                ++summary.schedulesCleared;
            }
        }
    }

    if (preservesAbsentIntensiveClasses)
    {
        for (const auto& [classId, times] : finalTimes)
        {
            (void)times;
            const Status cleared = m_database.execute(
                "DELETE FROM " + timeTable + " WHERE class_id=?",
                SqliteParameters{SqliteValue{std::int64_t{classId}}}
                );
            if (!cleared)
            {
                return std::unexpected(cleared.error());
            }
        }
    }
    else
    {
        const Status cleared = m_database.execute(
            "DELETE FROM " + timeTable
            );
        if (!cleared)
        {
            return std::unexpected(cleared.error());
        }
    }

    for (const auto& [classId, times] : finalTimes)
    {
        const Status written = writeTimes(
            m_database,
            timeTable,
            classId,
            times
            );
        if (!written)
        {
            return std::unexpected(written.error());
        }
    }

    if (plan.kind == ScheduleImportKind::Intensive)
    {
        const Status cleared = m_database.execute(
            "DELETE FROM intensive_slot_states"
            );
        if (!cleared)
        {
            return std::unexpected(cleared.error());
        }
        const Status written = writeIntensiveSlotStates(
            m_database,
            plan.intensiveSlotStates
            );
        if (!written)
        {
            return std::unexpected(written.error());
        }
    }

    if (plan.saveProfileNameIfBlank || plan.updateProfileName)
    {
        ApplicationSettingsService settings(m_database);
        const Result<SettingValue> current = settings.load("myInfo/name");
        if (!current)
        {
            return std::unexpected(current.error());
        }

        std::string existingName;
        if (const auto* text = std::get_if<std::string>(&*current))
        {
            existingName = trimAsciiWhitespace(*text);
        }
        else if (!std::holds_alternative<std::monostate>(*current))
        {
            return std::unexpected(error(
                ErrorCode::Schema,
                "SQLite returned a non-text app_settings value."
                ));
        }

        const std::string selectedName = trimAsciiWhitespace(
            plan.selectedUserName
            );
        if ((existingName.empty() || plan.updateProfileName)
            && !selectedName.empty())
        {
            const Status updated = settings.save(
                "myInfo/name",
                SettingValue{selectedName}
                );
            if (!updated)
            {
                return std::unexpected(updated.error());
            }
            summary.profileNameUpdated = true;
        }
    }

    const Status committed = transaction.commit();
    if (!committed)
    {
        return std::unexpected(committed.error());
    }
    return summary;
}

} // namespace classmngr::engine
