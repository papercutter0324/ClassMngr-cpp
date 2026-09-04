#include "classmngr/engine/sub_prep_class_information.h"

#include "classmngr/engine/class_info_config.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <map>
#include <string_view>
#include <utility>
#include <vector>

namespace classmngr::engine
{
namespace
{
constexpr int UnknownOrder = std::numeric_limits<int>::max();

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

int dayOrder(std::string_view day)
{
    const std::string normalized = trimAsciiWhitespace(day);
    const auto& days = ClassInfoConfig::days();
    const auto found = std::find(days.begin(), days.end(), normalized);
    return found == days.end()
        ? UnknownOrder
        : static_cast<int>(std::distance(days.begin(), found));
}

std::string dayAbbreviation(std::string_view day)
{
    if (day == "Monday")
    {
        return "Mon";
    }
    if (day == "Tuesday")
    {
        return "Tues";
    }
    if (day == "Wednesday")
    {
        return "Wed";
    }
    if (day == "Thursday")
    {
        return "Thurs";
    }
    if (day == "Friday")
    {
        return "Fri";
    }
    if (day == "Saturday")
    {
        return "Sat";
    }
    if (day == "Sunday")
    {
        return "Sun";
    }

    return trimAsciiWhitespace(day);
}

struct ParsedTime
{
    int totalSeconds = 0;
    bool valid = false;
};

bool isAsciiDigit(char character)
{
    return character >= '0' && character <= '9';
}

bool parseInteger(
    std::string_view value,
    int minimumDigits,
    int maximumDigits,
    int* result
    )
{
    if (!result
        || value.size() < static_cast<std::size_t>(minimumDigits)
        || value.size() > static_cast<std::size_t>(maximumDigits))
    {
        return false;
    }

    int parsed = 0;
    for (const char character : value)
    {
        if (!isAsciiDigit(character))
        {
            return false;
        }
        parsed = (parsed * 10) + (character - '0');
    }
    *result = parsed;
    return true;
}

ParsedTime parseTime(std::string_view value)
{
    std::string trimmed = trimAsciiWhitespace(value);
    if (trimmed.empty())
    {
        return {};
    }

    bool hasMeridiem = false;
    bool afternoon = false;
    if (trimmed.size() >= 2)
    {
        const char first = trimmed[trimmed.size() - 2];
        const char second = trimmed[trimmed.size() - 1];
        const char firstLower = static_cast<char>(
            std::tolower(static_cast<unsigned char>(first)));
        const char secondLower = static_cast<char>(
            std::tolower(static_cast<unsigned char>(second)));
        if ((firstLower == 'a' || firstLower == 'p')
            && secondLower == 'm')
        {
            hasMeridiem = true;
            afternoon = firstLower == 'p';
            trimmed = trimAsciiWhitespace(
                std::string_view(trimmed).substr(0, trimmed.size() - 2)
                );
        }
    }

    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (start <= trimmed.size())
    {
        const std::size_t separator = trimmed.find(':', start);
        const std::size_t end = separator == std::string::npos
            ? trimmed.size()
            : separator;
        parts.push_back(std::string_view(trimmed).substr(start, end - start));
        if (separator == std::string::npos)
        {
            break;
        }
        start = separator + 1;
    }

    if ((hasMeridiem && parts.size() != 2)
        || (!hasMeridiem && (parts.size() < 2 || parts.size() > 3)))
    {
        return {};
    }

    int hour = 0;
    int minute = 0;
    int second = 0;
    if (!parseInteger(parts[0], 1, 2, &hour)
        || !parseInteger(parts[1], 2, 2, &minute)
        || (parts.size() == 3
            && !parseInteger(parts[2], 2, 2, &second)))
    {
        return {};
    }

    if (minute > 59 || second > 59)
    {
        return {};
    }

    if (hasMeridiem)
    {
        if (hour < 1 || hour > 12)
        {
            return {};
        }
        if (hour == 12)
        {
            hour = 0;
        }
        if (afternoon)
        {
            hour += 12;
        }
    }
    else if (hour > 23)
    {
        return {};
    }

    return {
        (hour * 60 * 60) + (minute * 60) + second,
        true
    };
}

std::string compactTime(const ParsedTime& time)
{
    if (!time.valid)
    {
        return {};
    }

    const int hour24 = time.totalSeconds / (60 * 60);
    const int minute = (time.totalSeconds / 60) % 60;
    const int hour12 = (hour24 % 12) == 0 ? 12 : hour24 % 12;
    std::string result = std::to_string(hour12);
    if (minute != 0)
    {
        result += ':';
        if (minute < 10)
        {
            result += '0';
        }
        result += std::to_string(minute);
    }
    result += hour24 >= 12 ? "pm" : "am";
    return result;
}

bool contains(
    const std::vector<int>& values,
    int value
    )
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

bool contains(
    const std::vector<std::string>& values,
    std::string_view value
    )
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

std::string classLabel(const ClassInfo& info)
{
    const std::string grade = trimAsciiWhitespace(info.classGrade);
    const std::string level = trimAsciiWhitespace(info.classLevel);

    if (!grade.empty() && !level.empty())
    {
        return grade + ' ' + level;
    }
    if (!grade.empty())
    {
        return grade;
    }
    if (!level.empty())
    {
        return level;
    }
    return "N/A";
}

std::string teacherName(const Teacher& teacher)
{
    const std::string name = trimAsciiWhitespace(
        teacher.preferredDisplayName());
    return name.empty() ? "N/A" : name;
}

int gradeOrder(std::string_view grade)
{
    const std::string normalized = trimAsciiWhitespace(grade);
    const auto& grades = ClassInfoConfig::grades();
    const auto found = std::find(grades.begin(), grades.end(), normalized);
    return found == grades.end()
        ? UnknownOrder
        : static_cast<int>(std::distance(grades.begin(), found));
}

int levelOrder(const ClassInfo& info)
{
    const std::string grade = trimAsciiWhitespace(info.classGrade);
    const std::string level = trimAsciiWhitespace(info.classLevel);
    const std::vector<std::string> levels =
        ClassInfoConfig::levelsForGrade(grade);
    const auto found = std::find(levels.begin(), levels.end(), level);
    return found == levels.end()
        ? UnknownOrder
        : static_cast<int>(std::distance(levels.begin(), found));
}

std::pair<int, int> firstMeetingOrder(
    const std::vector<ClassTime>& times,
    const std::vector<std::string>& visibleDays
    )
{
    std::pair<int, int> result{UnknownOrder, UnknownOrder};

    for (const ClassTime& meeting : times)
    {
        if (!contains(visibleDays, meeting.day))
        {
            continue;
        }

        const ParsedTime time = parseTime(meeting.startTime);
        if (!time.valid)
        {
            continue;
        }

        const std::pair<int, int> candidate{
            dayOrder(meeting.day),
            time.totalSeconds
        };
        if (candidate < result)
        {
            result = candidate;
        }
    }

    return result;
}

const std::vector<ClassTime>& selectedTimes(
    const SubPrepClassDetails& details,
    const SubPrepBuildOptions& options
    )
{
    return options.useIntensive
        ? details.info.intensiveTimes
        : details.info.classTimes;
}
} // namespace

std::string SubPrepClassInformationService::formatMeetingTimes(
    const std::vector<ClassTime>& times,
    const std::vector<std::string>& visibleDays
    )
{
    struct Meeting
    {
        std::string day;
        ParsedTime time;
    };

    std::vector<Meeting> meetings;
    for (const ClassTime& source : times)
    {
        if (!contains(visibleDays, source.day))
        {
            continue;
        }

        const ParsedTime time = parseTime(source.startTime);
        if (time.valid)
        {
            meetings.push_back({source.day, time});
        }
    }

    std::sort(
        meetings.begin(),
        meetings.end(),
        [](const Meeting& left, const Meeting& right)
        {
            const int leftDay = dayOrder(left.day);
            const int rightDay = dayOrder(right.day);
            if (leftDay != rightDay)
            {
                return leftDay < rightDay;
            }
            return left.time.totalSeconds < right.time.totalSeconds;
        }
        );

    struct TimeGroup
    {
        ParsedTime time;
        std::vector<std::string> days;
        int firstDay = UnknownOrder;
    };

    std::vector<TimeGroup> groups;
    for (const Meeting& meeting : meetings)
    {
        auto group = std::find_if(
            groups.begin(),
            groups.end(),
            [&meeting](const TimeGroup& candidate)
            {
                return candidate.time.totalSeconds
                    == meeting.time.totalSeconds;
            }
            );

        if (group == groups.end())
        {
            groups.push_back({
                meeting.time,
                {meeting.day},
                dayOrder(meeting.day)
            });
        }
        else if (std::find(
                     group->days.begin(),
                     group->days.end(),
                     meeting.day
                     ) == group->days.end())
        {
            group->days.push_back(meeting.day);
        }
    }

    std::sort(
        groups.begin(),
        groups.end(),
        [](const TimeGroup& left, const TimeGroup& right)
        {
            if (left.firstDay != right.firstDay)
            {
                return left.firstDay < right.firstDay;
            }
            return left.time.totalSeconds < right.time.totalSeconds;
        }
        );

    std::vector<std::string> labels;
    for (const TimeGroup& group : groups)
    {
        std::string days;
        for (const std::string& day : group.days)
        {
            days += dayAbbreviation(day);
        }
        labels.push_back(days + ' ' + compactTime(group.time));
    }

    if (labels.empty())
    {
        return "N/A";
    }

    std::string result = labels.front();
    for (std::size_t index = 1; index < labels.size(); ++index)
    {
        result += " & ";
        result += labels[index];
    }
    return result;
}

std::vector<SubPrepTeacherGroup> SubPrepClassInformationService::build(
    const std::vector<SubPrepSourceClass>& sourceClasses,
    const SubPrepBuildOptions& options
    )
{
    std::map<int, std::vector<SubPrepClassDetails>> classesByTeacher;
    std::map<int, Teacher> teachersById;
    std::vector<int> processedClassIds;

    for (const SubPrepSourceClass& source : sourceClasses)
    {
        const int classId = source.classroom.id;
        const int teacherId = source.info.teacherId;
        if (classId <= 0
            || teacherId <= 0
            || source.teacher.id <= 0
            || !contains(options.visibleClassIds, classId)
            || contains(processedClassIds, classId))
        {
            continue;
        }

        processedClassIds.push_back(classId);
        teachersById[teacherId] = source.teacher;

        const std::vector<ClassTime>& selected = options.useIntensive
            ? source.info.intensiveTimes
            : source.info.classTimes;

        SubPrepClassDetails details;
        details.classId = classId;
        details.info = source.info;
        details.studentCount = source.studentCount;
        details.classLabel = classLabel(source.info);
        details.timeText = formatMeetingTimes(
            selected,
            options.visibleDays
            );
        classesByTeacher[teacherId].push_back(std::move(details));
    }

    std::vector<Teacher> teachers;
    teachers.reserve(teachersById.size());
    for (const auto& [teacherId, teacher] : teachersById)
    {
        (void)teacherId;
        teachers.push_back(teacher);
    }
    std::sort(
        teachers.begin(),
        teachers.end(),
        classmngr::engine::teacherDisplayLessThan
        );

    std::vector<SubPrepTeacherGroup> groups;
    groups.reserve(teachers.size());
    for (const Teacher& teacher : teachers)
    {
        std::vector<SubPrepClassDetails> classes = classesByTeacher[teacher.id];
        std::sort(
            classes.begin(),
            classes.end(),
            [&options](
                const SubPrepClassDetails& left,
                const SubPrepClassDetails& right
                )
            {
                const int leftGrade = gradeOrder(left.info.classGrade);
                const int rightGrade = gradeOrder(right.info.classGrade);
                if (leftGrade != rightGrade)
                {
                    return leftGrade < rightGrade;
                }

                const int leftLevel = levelOrder(left.info);
                const int rightLevel = levelOrder(right.info);
                if (leftLevel != rightLevel)
                {
                    return leftLevel < rightLevel;
                }

                const auto leftMeeting = firstMeetingOrder(
                    selectedTimes(left, options),
                    options.visibleDays
                    );
                const auto rightMeeting = firstMeetingOrder(
                    selectedTimes(right, options),
                    options.visibleDays
                    );
                if (leftMeeting != rightMeeting)
                {
                    return leftMeeting < rightMeeting;
                }

                return left.classId < right.classId;
            }
            );

        std::vector<std::string> classLabels;
        for (const SubPrepClassDetails& details : classes)
        {
            if (std::find(
                    classLabels.begin(),
                    classLabels.end(),
                    details.classLabel
                    ) == classLabels.end())
            {
                classLabels.push_back(details.classLabel);
            }
        }

        std::string classListText;
        for (const std::string& label : classLabels)
        {
            if (!classListText.empty())
            {
                classListText += " / ";
            }
            classListText += label;
        }

        SubPrepTeacherGroup group;
        group.teacher = teacher;
        group.displayName = teacherName(teacher);
        group.classListText = std::move(classListText);
        group.classes = std::move(classes);
        groups.push_back(std::move(group));
    }

    return groups;
}

} // namespace classmngr::engine
