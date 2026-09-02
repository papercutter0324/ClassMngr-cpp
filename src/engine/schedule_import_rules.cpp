#include "classmngr/engine/schedule_import_rules.h"

#include "classmngr/engine/class_info_config.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace classmngr::engine
{
namespace
{
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

std::string normalized(std::string_view value)
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
        result.push_back(
            character >= 'A' && character <= 'Z'
                ? static_cast<char>(character - 'A' + 'a')
                : character
            );
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

struct MeetingPatternPolicy
{
    ScheduleImportMeetingPatternExpectation expectation =
        ScheduleImportMeetingPatternExpectation::Unsupported;
    std::vector<std::vector<std::string>> allowedPatterns;
};

MeetingPatternPolicy policyFor(
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
            ScheduleImportMeetingPatternExpectation::WeekdayPairs,
            {
                mondayWednesday,
                mondayFriday,
                wednesdayFriday,
                tuesdayThursday
            }
        };
    }
    if (grade == "E5" && equalsInsensitive(level, "Athena"))
    {
        return {
            ScheduleImportMeetingPatternExpectation::
                WeekdayTripleOrTuesdayThursday,
            {mondayWednesdayFriday, tuesdayThursday}
        };
    }
    if (grade == "E6" && songs)
    {
        return {
            ScheduleImportMeetingPatternExpectation::
                WeekdayTripleOrTuesdayThursday,
            {mondayWednesdayFriday, tuesdayThursday}
        };
    }
    if ((grade == "M1" || grade == "M2" || grade == "M3") && songs)
    {
        return {
            ScheduleImportMeetingPatternExpectation::WeekdayPairs,
            {
                mondayWednesday,
                mondayFriday,
                wednesdayFriday,
                tuesdayThursday
            }
        };
    }
    if (grade == "E6" || grade == "M1" || grade == "M2")
    {
        return {
            ScheduleImportMeetingPatternExpectation::OneWeekday,
            {
                {"Monday"},
                {"Tuesday"},
                {"Wednesday"},
                {"Thursday"},
                {"Friday"}
            }
        };
    }

    return {};
}

std::string patternKey(
    std::vector<std::string> days
    )
{
    std::sort(
        days.begin(),
        days.end(),
        [](const std::string& left, const std::string& right)
        {
            const auto index = [](const std::string& day)
            {
                const std::string normalizedDay = normalized(day);
                for (std::size_t position = 0;
                     position < ClassInfoConfig::days().size();
                     ++position)
                {
                    if (normalized(ClassInfoConfig::days()[position])
                        == normalizedDay)
                    {
                        return static_cast<int>(position);
                    }
                }
                return -1;
            };
            return index(left) < index(right);
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
} // namespace

int ScheduleImportRules::weekdayIndex(
    std::string_view day
    )
{
    const std::string normalizedDay = normalized(day);
    for (std::size_t index = 0;
         index < ClassInfoConfig::days().size();
         ++index)
    {
        if (normalized(ClassInfoConfig::days()[index]) == normalizedDay)
        {
            return static_cast<int>(index);
        }
    }
    return -1;
}

int ScheduleImportRules::dayGroup(
    const std::vector<ClassTime>& times
    )
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

bool ScheduleImportRules::daysAreCompatible(
    const std::vector<ClassTime>& importedTimes,
    const std::vector<ClassTime>& existingTimes
    )
{
    const int importedGroup = dayGroup(importedTimes);
    return importedGroup != 0 && importedGroup == dayGroup(existingTimes);
}

std::vector<std::string> ScheduleImportRules::meetingDays(
    const std::vector<ClassTime>& times
    )
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

bool ScheduleImportRules::meetingDaysMatch(
    const std::vector<ClassTime>& importedTimes,
    const std::vector<ClassTime>& existingTimes
    )
{
    return daysAreCompatible(importedTimes, existingTimes)
        && meetingDays(importedTimes) == meetingDays(existingTimes);
}

std::vector<ClassTime> ScheduleImportRules::timesForKind(
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

const std::vector<ClassTime>& ScheduleImportRules::targetTimesForKind(
    const ClassInfo& info,
    ScheduleImportKind kind
    )
{
    return kind == ScheduleImportKind::Intensive
        ? info.intensiveTimes
        : info.classTimes;
}

bool ScheduleImportRules::classOptionIsEligible(
    const ScheduleImportClassCandidate& candidate,
    const ClassInfo& existing,
    ScheduleImportKind kind
    )
{
    const std::vector<ClassTime> referenceTimes = timesForKind(existing, kind);
    return equalsInsensitive(candidate.classGrade, existing.classGrade)
        && equalsInsensitive(candidate.classLevel, existing.classLevel)
        && (
            referenceTimes.empty()
            || daysAreCompatible(candidate.times, referenceTimes)
            );
}

ScheduleImportMeetingPatternExpectation
ScheduleImportRules::meetingPatternExpectation(
    std::string_view classGrade,
    std::string_view classLevel
    )
{
    return policyFor(classGrade, classLevel).expectation;
}

std::vector<std::vector<std::string>>
ScheduleImportRules::allowedDayPatterns(
    std::string_view classGrade,
    std::string_view classLevel
    )
{
    return policyFor(classGrade, classLevel).allowedPatterns;
}

ScheduleImportMeetingPatternResult
ScheduleImportRules::validateMeetingPattern(
    const ScheduleImportClassCandidate& candidate
    )
{
    const MeetingPatternPolicy policy = policyFor(
        candidate.classGrade,
        candidate.classLevel
        );

    ScheduleImportMeetingPatternResult result;
    result.expectation = policy.expectation;
    for (const ClassTime& time : candidate.times)
    {
        const int day = weekdayIndex(time.day);
        if (day < 0 || day > 4
            || std::find(
                result.meetingDays.begin(),
                result.meetingDays.end(),
                ClassInfoConfig::days()[static_cast<std::size_t>(day)]
                ) != result.meetingDays.end())
        {
            result.status =
                ScheduleImportMeetingPatternStatus::InvalidWeekdayOrDuplicate;
            return result;
        }
        result.meetingDays.push_back(
            ClassInfoConfig::days()[static_cast<std::size_t>(day)]
            );
    }

    if (policy.allowedPatterns.empty())
    {
        return result;
    }

    const std::string key = patternKey(result.meetingDays);
    for (const std::vector<std::string>& pattern : policy.allowedPatterns)
    {
        if (patternKey(pattern) == key)
        {
            return result;
        }
    }

    result.status = ScheduleImportMeetingPatternStatus::UnsupportedPattern;
    return result;
}

} // namespace classmngr::engine
