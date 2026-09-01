#include "classmngr/engine/class_tab_navigation.h"

#include "classmngr/engine/class_info_config.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace classmngr::engine
{
namespace
{
constexpr int UnknownOrder = 1000;
constexpr std::string_view Bullet = "\xE2\x80\xA2";

using Service = ClassTabNavigationService;

struct ParsedTime
{
    int hour = 0;
    int minute = 0;
    bool usesMeridiem = false;
    bool isPm = false;
};

struct TabCandidate
{
    Service::ClassTab tab;
    std::string baseLabel;
    std::string teacherLabel;
};

struct TimeGroup
{
    std::string startTime;
    std::vector<std::string> days;
};

bool isAsciiSpace(char value)
{
    return std::isspace(static_cast<unsigned char>(value)) != 0;
}

std::string trimmed(std::string_view value)
{
    std::size_t first = 0;
    while (first < value.size() && isAsciiSpace(value[first]))
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first && isAsciiSpace(value[last - 1]))
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

bool containsAsciiInsensitive(
    std::string_view value,
    std::string_view needle
    )
{
    if (needle.empty())
    {
        return true;
    }

    if (needle.size() > value.size())
    {
        return false;
    }

    for (std::size_t offset = 0;
         offset + needle.size() <= value.size();
         ++offset)
    {
        if (equalsAsciiInsensitive(value.substr(offset, needle.size()), needle))
        {
            return true;
        }
    }

    return false;
}

std::string removeAsciiInsensitive(
    std::string value,
    std::string_view needle
    )
{
    if (needle.empty())
    {
        return value;
    }

    std::string result;
    result.reserve(value.size());

    for (std::size_t offset = 0; offset < value.size();)
    {
        if (offset + needle.size() <= value.size()
            && equalsAsciiInsensitive(
                std::string_view(value).substr(offset, needle.size()),
                needle
                ))
        {
            offset += needle.size();
            continue;
        }

        result.push_back(value[offset]);
        ++offset;
    }

    return result;
}

bool parseDigits(
    std::string_view value,
    std::size_t* position,
    int minimumDigits,
    int maximumDigits,
    int* result
    )
{
    if (!position || !result)
    {
        return false;
    }

    const std::size_t start = *position;
    int parsed = 0;
    int count = 0;

    while (*position < value.size()
           && count < maximumDigits
           && value[*position] >= '0'
           && value[*position] <= '9')
    {
        parsed = (parsed * 10) + (value[*position] - '0');
        ++*position;
        ++count;
    }

    if (count < minimumDigits)
    {
        *position = start;
        return false;
    }

    *result = parsed;
    return true;
}

std::optional<ParsedTime> parseStartTime(std::string_view value)
{
    const std::string input = trimmed(value);
    if (input.empty())
    {
        return std::nullopt;
    }

    std::size_t position = 0;
    int hour = 0;
    int minute = 0;
    if (!parseDigits(input, &position, 1, 2, &hour)
        || position >= input.size()
        || input[position] != ':')
    {
        return std::nullopt;
    }
    ++position;

    if (!parseDigits(input, &position, 2, 2, &minute))
    {
        return std::nullopt;
    }

    bool hasSeconds = false;
    if (position < input.size() && input[position] == ':')
    {
        hasSeconds = true;
        ++position;
        int seconds = 0;
        if (!parseDigits(input, &position, 2, 2, &seconds)
            || seconds > 59)
        {
            return std::nullopt;
        }
    }

    while (position < input.size() && input[position] == ' ')
    {
        ++position;
    }

    ParsedTime result;
    result.hour = hour;
    result.minute = minute;

    if (position < input.size())
    {
        const std::string_view meridiem =
            std::string_view(input).substr(position);
        if (hasSeconds || (!equalsAsciiInsensitive(meridiem, "AM")
                           && !equalsAsciiInsensitive(meridiem, "PM")))
        {
            return std::nullopt;
        }

        result.usesMeridiem = true;
        result.isPm = equalsAsciiInsensitive(meridiem, "PM");
        if (hour < 1 || hour > 12)
        {
            return std::nullopt;
        }
    }
    else if (hour > 23)
    {
        return std::nullopt;
    }

    if (minute > 59)
    {
        return std::nullopt;
    }

    return result;
}

std::string twoDigits(int value)
{
    std::string result = std::to_string(value);
    if (result.size() == 1)
    {
        result.insert(result.begin(), '0');
    }
    return result;
}

std::string compactStartTime(std::string_view value)
{
    const std::string input = trimmed(value);
    if (input.empty())
    {
        return {};
    }

    const std::optional<ParsedTime> parsed = parseStartTime(input);
    const bool usesMeridiem =
        containsAsciiInsensitive(input, "AM")
        || containsAsciiInsensitive(input, "PM");

    if (parsed)
    {
        if (usesMeridiem)
        {
            int displayHour = parsed->hour % 12;
            if (displayHour == 0)
            {
                displayHour = 12;
            }

            return std::to_string(displayHour)
                + ':'
                + twoDigits(parsed->minute);
        }

        return std::to_string(parsed->hour)
            + ':'
            + twoDigits(parsed->minute);
    }

    std::string fallback = input;
    fallback = removeAsciiInsensitive(std::move(fallback), " AM");
    fallback = removeAsciiInsensitive(std::move(fallback), " PM");
    return fallback;
}

int gradeOrder(std::string_view grade)
{
    const std::string normalized = trimmed(grade);
    const auto& grades = ClassInfoConfig::grades();
    const auto found = std::find(grades.begin(), grades.end(), normalized);
    return found == grades.end()
        ? UnknownOrder
        : static_cast<int>(std::distance(grades.begin(), found));
}

int levelOrder(
    std::string_view grade,
    std::string_view level
    )
{
    const std::string normalizedGrade = trimmed(grade);
    const std::string normalizedLevel = trimmed(level);
    const auto levels = ClassInfoConfig::levelsForGrade(normalizedGrade);
    const auto found =
        std::find(levels.begin(), levels.end(), normalizedLevel);
    return found == levels.end()
        ? UnknownOrder
        : static_cast<int>(std::distance(levels.begin(), found));
}

std::string gradeKey(const Service::ClassEntry& entry)
{
    const std::string grade = trimmed(entry.grade);
    return gradeOrder(grade) == UnknownOrder ? std::string{} : grade;
}

std::string gradeLabel(
    std::string_view key,
    const Service::Labels& labels
    )
{
    const std::string normalized = trimmed(key);
    return normalized.empty() ? labels.other : normalized;
}

std::string dayCode(std::string_view day)
{
    const std::string normalized = trimmed(day);
    if (normalized == "Monday")
    {
        return "M";
    }
    if (normalized == "Tuesday")
    {
        return "T";
    }
    if (normalized == "Wednesday")
    {
        return "W";
    }
    if (normalized == "Thursday")
    {
        return "Th";
    }
    if (normalized == "Friday")
    {
        return "F";
    }
    if (normalized == "Saturday")
    {
        return "Sat";
    }
    if (normalized == "Sunday")
    {
        return "Sun";
    }

    return normalized;
}

int dayOrder(std::string_view day)
{
    const std::string normalized = trimmed(day);
    const auto& days = ClassInfoConfig::days();
    const auto found = std::find(days.begin(), days.end(), normalized);
    return found == days.end()
        ? UnknownOrder
        : static_cast<int>(std::distance(days.begin(), found));
}

void appendUnique(
    std::vector<std::string>* values,
    std::string value
    )
{
    if (!values)
    {
        return;
    }

    if (std::find(values->begin(), values->end(), value) == values->end())
    {
        values->push_back(std::move(value));
    }
}

std::string compressedDays(std::vector<std::string> days)
{
    std::vector<std::string> uniqueDays;
    uniqueDays.reserve(days.size());
    for (std::string& day : days)
    {
        appendUnique(&uniqueDays, std::move(day));
    }

    std::stable_sort(
        uniqueDays.begin(),
        uniqueDays.end(),
        [](const std::string& left, const std::string& right)
        {
            return dayOrder(left) < dayOrder(right);
        }
        );

    std::vector<std::string> codes;
    codes.reserve(uniqueDays.size());
    for (const std::string& day : uniqueDays)
    {
        const std::string code = dayCode(day);
        if (!code.empty())
        {
            codes.push_back(code);
        }
    }

    if (codes == std::vector<std::string>{"M", "W"})
    {
        return "M/W";
    }
    if (codes == std::vector<std::string>{"M", "F"})
    {
        return "M/F";
    }
    if (codes == std::vector<std::string>{"W", "F"})
    {
        return "W/F";
    }
    if (codes == std::vector<std::string>{"M", "W", "F"})
    {
        return "M/W/F";
    }
    if (codes == std::vector<std::string>{"T", "Th"})
    {
        return "T/Th";
    }

    std::string result;
    for (const std::string& code : codes)
    {
        if (!result.empty())
        {
            result += '/';
        }
        result += code;
    }
    return result;
}

std::string scheduleText(const std::vector<ClassTime>& times)
{
    if (times.empty())
    {
        return {};
    }

    std::vector<TimeGroup> groups;
    for (const ClassTime& time : times)
    {
        const std::string start = compactStartTime(time.startTime);
        if (start.empty())
        {
            continue;
        }

        auto group = std::find_if(
            groups.begin(),
            groups.end(),
            [&start](const TimeGroup& candidate)
            {
                return candidate.startTime == start;
            }
            );

        if (group == groups.end())
        {
            groups.push_back({start, {trimmed(time.day)}});
        }
        else
        {
            group->days.push_back(trimmed(time.day));
        }
    }

    std::string result;
    for (const TimeGroup& group : groups)
    {
        if (!result.empty())
        {
            result += "; ";
        }
        result += compressedDays(group.days);
        result += ' ';
        result += group.startTime;
    }
    return result;
}

const std::vector<ClassTime>& preferredTimes(const Service::ClassEntry& entry)
{
    return entry.regularTimes.empty()
        ? entry.intensiveTimes
        : entry.regularTimes;
}

const std::vector<ClassTime>& timesForFilter(
    const Service::ClassEntry& entry,
    Service::ScheduleSource scheduleSource
    )
{
    return scheduleSource == Service::ScheduleSource::Intensive
        ? entry.intensiveTimes
        : entry.regularTimes;
}

std::string normalizedDay(std::string_view day)
{
    std::string result = trimmed(day);
    for (char& character : result)
    {
        character = lowerAscii(character);
    }
    return result;
}

std::vector<std::string> expandedFilterDays(const Service::DayFilter& filter)
{
    std::vector<std::string> result;
    for (const std::string& day : filter.selectedDays)
    {
        const std::string normalized = normalizedDay(day);
        if (normalized == "wkend" || normalized == "weekend")
        {
            appendUnique(&result, "saturday");
            appendUnique(&result, "sunday");
        }
        else if (!normalized.empty())
        {
            appendUnique(&result, normalized);
        }
    }
    return result;
}

bool contains(
    const std::vector<std::string>& values,
    std::string_view value
    )
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

bool matchesDayFilter(
    const Service::ClassEntry& entry,
    const Service::DayFilter& filter
    )
{
    const std::vector<ClassTime>& scheduleTimes =
        timesForFilter(entry, filter.scheduleSource);

    if (filter.visibilityScope == Service::VisibilityScope::ActiveSchedule
        && scheduleTimes.empty())
    {
        return false;
    }

    const std::vector<std::string> selectedDays = expandedFilterDays(filter);
    if (selectedDays.empty())
    {
        return true;
    }

    for (const ClassTime& time : scheduleTimes)
    {
        if (contains(selectedDays, normalizedDay(time.day)))
        {
            return true;
        }
    }

    return false;
}

std::vector<Service::ClassEntry> filteredEntries(
    const std::vector<Service::ClassEntry>& entries,
    const Service::DayFilter& filter
    )
{
    std::vector<Service::ClassEntry> result;
    result.reserve(entries.size());
    for (const Service::ClassEntry& entry : entries)
    {
        if (matchesDayFilter(entry, filter))
        {
            result.push_back(entry);
        }
    }
    return result;
}

int timeOrder(std::string_view value)
{
    const std::optional<ParsedTime> parsed = parseStartTime(value);
    if (!parsed)
    {
        return UnknownOrder;
    }

    int hour = parsed->hour;
    if (parsed->usesMeridiem)
    {
        hour %= 12;
        if (parsed->isPm)
        {
            hour += 12;
        }
    }
    return (hour * 60) + parsed->minute;
}

int firstDayOrder(const Service::ClassEntry& entry)
{
    int result = UnknownOrder;
    for (const ClassTime& time : preferredTimes(entry))
    {
        result = std::min(result, dayOrder(time.day));
    }
    return result;
}

int firstTimeOrder(const Service::ClassEntry& entry)
{
    const int firstDay = firstDayOrder(entry);
    int result = UnknownOrder;
    for (const ClassTime& time : preferredTimes(entry))
    {
        if (dayOrder(time.day) == firstDay)
        {
            result = std::min(result, timeOrder(time.startTime));
        }
    }
    return result;
}

std::string replaceFirst(
    std::string value,
    std::string_view placeholder,
    std::string_view replacement
    )
{
    const std::size_t position = value.find(placeholder);
    if (position != std::string::npos)
    {
        value.replace(position, placeholder.size(), replacement);
    }
    return value;
}

std::string preferredScheduleText(
    const Service::ClassEntry& entry,
    const Service::Labels& labels
    )
{
    const std::string regular = scheduleText(entry.regularTimes);
    if (!regular.empty())
    {
        return regular;
    }

    const std::string intensive = scheduleText(entry.intensiveTimes);
    if (!intensive.empty())
    {
        return labels.intensive + ' ' + intensive;
    }

    return labels.noTime;
}

std::string classNameText(
    const Service::ClassEntry& entry,
    bool includeGrade,
    const Service::Labels& labels
    )
{
    const std::string grade = trimmed(entry.grade);
    const std::string level = trimmed(entry.level);

    if (includeGrade)
    {
        if (!grade.empty() && !level.empty())
        {
            return grade + ' ' + level;
        }
        if (!grade.empty())
        {
            return grade;
        }
    }

    if (!level.empty())
    {
        return level;
    }
    if (!grade.empty())
    {
        return grade;
    }

    const std::string classroomName = trimmed(entry.classroomName);
    if (!classroomName.empty())
    {
        return classroomName;
    }

    return replaceFirst(
        labels.classFallback,
        "%1",
        std::to_string(entry.classId)
        );
}

std::string baseLabel(
    const Service::ClassEntry& entry,
    bool includeGrade,
    const Service::Labels& labels
    )
{
    return classNameText(entry, includeGrade, labels)
        + ' '
        + std::string(Bullet)
        + ' '
        + preferredScheduleText(entry, labels);
}

std::string teacherLabel(const Service::ClassEntry& entry)
{
    const std::string english = trimmed(entry.teacherEn);
    return english.empty() ? trimmed(entry.teacherKr) : english;
}

bool entryLessThan(
    const Service::ClassEntry& left,
    const Service::ClassEntry& right,
    const Service::Labels& labels
    )
{
    const int leftGradeOrder = gradeOrder(left.grade);
    const int rightGradeOrder = gradeOrder(right.grade);
    if (leftGradeOrder != rightGradeOrder)
    {
        return leftGradeOrder < rightGradeOrder;
    }

    const int leftLevelOrder = levelOrder(left.grade, left.level);
    const int rightLevelOrder = levelOrder(right.grade, right.level);
    if (leftLevelOrder != rightLevelOrder)
    {
        return leftLevelOrder < rightLevelOrder;
    }

    const int leftDayOrder = firstDayOrder(left);
    const int rightDayOrder = firstDayOrder(right);
    if (leftDayOrder != rightDayOrder)
    {
        return leftDayOrder < rightDayOrder;
    }

    const int leftTimeOrder = firstTimeOrder(left);
    const int rightTimeOrder = firstTimeOrder(right);
    if (leftTimeOrder != rightTimeOrder)
    {
        return leftTimeOrder < rightTimeOrder;
    }

    const std::string leftLabel = baseLabel(left, true, labels);
    const std::string rightLabel = baseLabel(right, true, labels);
    if (leftLabel != rightLabel)
    {
        return leftLabel < rightLabel;
    }

    return left.classId < right.classId;
}

std::vector<Service::ClassEntry> sortedEntries(
    const std::vector<Service::ClassEntry>& entries,
    const Service::Labels& labels
    )
{
    std::vector<Service::ClassEntry> result = entries;
    std::sort(
        result.begin(),
        result.end(),
        [&labels](
            const Service::ClassEntry& left,
            const Service::ClassEntry& right
            )
        {
            return entryLessThan(left, right, labels);
        }
        );
    return result;
}

void applyUniqueLabels(std::vector<TabCandidate>* candidates)
{
    if (!candidates)
    {
        return;
    }

    for (std::size_t index = 0; index < candidates->size(); ++index)
    {
        TabCandidate& candidate = (*candidates)[index];
        int duplicateCount = 0;
        for (const TabCandidate& other : *candidates)
        {
            if (other.baseLabel == candidate.baseLabel)
            {
                ++duplicateCount;
            }
        }

        if (duplicateCount <= 1)
        {
            candidate.tab.label = candidate.baseLabel;
            continue;
        }

        std::string expandedLabel = candidate.baseLabel;
        if (!candidate.teacherLabel.empty())
        {
            expandedLabel += ' ';
            expandedLabel += Bullet;
            expandedLabel += ' ';
            expandedLabel += candidate.teacherLabel;
        }

        bool stillDuplicated = candidate.teacherLabel.empty();
        if (!stillDuplicated)
        {
            for (std::size_t otherIndex = 0;
                 otherIndex < candidates->size();
                 ++otherIndex)
            {
                if (otherIndex == index)
                {
                    continue;
                }

                const TabCandidate& other = (*candidates)[otherIndex];
                if (other.baseLabel != candidate.baseLabel)
                {
                    continue;
                }

                std::string otherExpandedLabel = other.baseLabel;
                if (!other.teacherLabel.empty())
                {
                    otherExpandedLabel += ' ';
                    otherExpandedLabel += Bullet;
                    otherExpandedLabel += ' ';
                    otherExpandedLabel += other.teacherLabel;
                }

                if (otherExpandedLabel == expandedLabel)
                {
                    stillDuplicated = true;
                    break;
                }
            }
        }

        candidate.tab.label = stillDuplicated
            ? expandedLabel + " #" + std::to_string(candidate.tab.classId)
            : expandedLabel;
    }
}

std::vector<Service::ClassTab> makeClassTabs(
    const std::vector<Service::ClassEntry>& entries,
    bool includeGrade,
    const Service::Labels& labels
    )
{
    std::vector<TabCandidate> candidates;
    candidates.reserve(entries.size());

    for (const Service::ClassEntry& entry : entries)
    {
        TabCandidate candidate;
        candidate.tab.classId = entry.classId;
        candidate.baseLabel = baseLabel(entry, includeGrade, labels);
        candidate.teacherLabel = teacherLabel(entry);
        candidates.push_back(std::move(candidate));
    }

    applyUniqueLabels(&candidates);

    std::vector<Service::ClassTab> tabs;
    tabs.reserve(candidates.size());
    for (const TabCandidate& candidate : candidates)
    {
        tabs.push_back(candidate.tab);
    }
    return tabs;
}
} // namespace

ClassTabNavigationService::Model ClassTabNavigationService::build(
    const std::vector<ClassEntry>& entries,
    GroupingPolicy groupingPolicy,
    const DayFilter& dayFilter,
    const Labels& labels
    )
{
    const std::vector<ClassEntry> filtered =
        filteredEntries(entries, dayFilter);

    Model model;
    model.mode = groupingPolicy == GroupingPolicy::AlwaysGradeGrouped
        || filtered.size() > static_cast<std::size_t>(FlatClassThreshold)
        ? Mode::GradeGrouped
        : Mode::Flat;

    const std::vector<ClassEntry> sorted = sortedEntries(filtered, labels);
    model.allClasses = makeClassTabs(sorted, true, labels);

    if (model.mode == Mode::Flat)
    {
        model.flatClasses = model.allClasses;
        return model;
    }

    std::vector<std::string> groupKeys;
    for (const ClassEntry& entry : sorted)
    {
        appendUnique(&groupKeys, gradeKey(entry));
    }

    std::sort(
        groupKeys.begin(),
        groupKeys.end(),
        [](const std::string& left, const std::string& right)
        {
            const int leftOrder = left.empty() ? UnknownOrder : gradeOrder(left);
            const int rightOrder =
                right.empty() ? UnknownOrder : gradeOrder(right);
            return leftOrder < rightOrder;
        }
        );

    for (const std::string& key : groupKeys)
    {
        std::vector<ClassEntry> groupEntries;
        for (const ClassEntry& entry : sorted)
        {
            if (gradeKey(entry) == key)
            {
                groupEntries.push_back(entry);
            }
        }

        GradeGroup group;
        group.grade = key;
        group.label = gradeLabel(key, labels);
        group.classes = makeClassTabs(groupEntries, false, labels);
        model.gradeGroups.push_back(std::move(group));
    }

    return model;
}

} // namespace classmngr::engine
