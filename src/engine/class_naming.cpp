#include "classmngr/engine/class_naming.h"

#include <cctype>
#include <string_view>
#include <vector>

namespace classmngr::engine
{
namespace
{
constexpr std::string_view Bullet = "\xE2\x80\xA2";

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

std::string lowerAscii(std::string_view value)
{
    std::string result(value);
    for (char& character : result)
    {
        if (character >= 'A' && character <= 'Z')
        {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }
    return result;
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
    return std::string(day);
}

void replaceAll(
    std::string& value,
    std::string_view source,
    std::string_view replacement
    )
{
    std::size_t offset = 0;
    while ((offset = value.find(source, offset)) != std::string::npos)
    {
        value.replace(offset, source.size(), replacement);
        offset += replacement.size();
    }
}

std::string classLevelText(const ClassInfo& info)
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
    return "Unknown Class";
}

bool isSpecialDayPattern(
    const std::vector<std::string>& days,
    std::string* label
    )
{
    if (!label)
    {
        return false;
    }

    if (days == std::vector<std::string>{"Mon", "Wed"})
    {
        *label = "M/W";
        return true;
    }
    if (days == std::vector<std::string>{"Mon", "Fri"})
    {
        *label = "M/F";
        return true;
    }
    if (days == std::vector<std::string>{"Wed", "Fri"})
    {
        *label = "W/F";
        return true;
    }
    if (days == std::vector<std::string>{"Mon", "Wed", "Fri"})
    {
        *label = "M/W/F";
        return true;
    }
    if (days == std::vector<std::string>{"Tues", "Thurs"})
    {
        *label = "T/Th";
        return true;
    }
    return false;
}

std::string formatClassDays(
    const std::vector<std::string>& dayLabels
    )
{
    std::string special;
    if (isSpecialDayPattern(dayLabels, &special))
    {
        return special;
    }

    std::string result;
    for (std::size_t index = 0; index < dayLabels.size(); ++index)
    {
        result += dayLabels[index];
        if (index + 1 < dayLabels.size())
        {
            result += '/';
        }
    }
    return result;
}

std::string formatClassTimes(
    const std::vector<std::string>& timeLabels
    )
{
    std::vector<std::string> uniqueTimes;
    for (const std::string& time : timeLabels)
    {
        bool seen = false;
        for (const std::string& unique : uniqueTimes)
        {
            if (unique == time)
            {
                seen = true;
                break;
            }
        }
        if (!seen)
        {
            uniqueTimes.push_back(time);
        }
    }

    if (uniqueTimes.size() == 1)
    {
        return uniqueTimes.front();
    }

    std::string result;
    for (const std::string& time : timeLabels)
    {
        if (!result.empty())
        {
            result += " / ";
        }
        result += time;
    }
    return result;
}
} // namespace

std::string ClassNamingService::classDisplayName(
    const ClassInfo& info,
    const Teacher& teacher
    )
{
    const std::string levelText = classLevelText(info);
    std::string teacherName = teacher.preferredDisplayName();
    if (teacherName.empty())
    {
        teacherName = "No Teacher";
    }

    if (info.classTimes.empty())
    {
        return levelText + ' ' + Bullet.data() + ' ' + teacherName;
    }

    std::vector<std::string> dayLabels;
    std::vector<std::string> timeLabels;
    dayLabels.reserve(info.classTimes.size());
    timeLabels.reserve(info.classTimes.size());
    for (const ClassTime& time : info.classTimes)
    {
        dayLabels.push_back(dayAbbreviation(time.day));
        std::string startTime = time.startTime;
        replaceAll(startTime, " AM", "");
        replaceAll(startTime, " PM", "");
        timeLabels.push_back(std::move(startTime));
    }

    return levelText + ' ' + Bullet.data() + ' ' + teacherName
        + ' ' + Bullet.data() + ' ' + formatClassDays(dayLabels)
        + " (" + formatClassTimes(timeLabels) + ')';
}

std::string ClassNamingService::teacherDisplayName(const Teacher& teacher)
{
    const std::string name = teacher.preferredDisplayName();
    return name.empty() ? "New Teacher" : name;
}

bool ClassNamingService::teacherDisplayLessThan(
    const Teacher& left,
    const Teacher& right
    )
{
    const std::string leftEnglish = trimAsciiWhitespace(left.teacherEn);
    const std::string rightEnglish = trimAsciiWhitespace(right.teacherEn);
    const bool leftHasEnglish = !leftEnglish.empty();
    const bool rightHasEnglish = !rightEnglish.empty();
    if (leftHasEnglish != rightHasEnglish)
    {
        return leftHasEnglish;
    }

    const std::string leftFolded = lowerAscii(leftEnglish);
    const std::string rightFolded = lowerAscii(rightEnglish);
    if (leftFolded != rightFolded)
    {
        return leftFolded < rightFolded;
    }
    if (leftEnglish != rightEnglish)
    {
        return leftEnglish < rightEnglish;
    }

    const std::string leftKorean = trimAsciiWhitespace(left.teacherKr);
    const std::string rightKorean = trimAsciiWhitespace(right.teacherKr);
    if (leftKorean != rightKorean)
    {
        return leftKorean < rightKorean;
    }
    return left.id < right.id;
}

} // namespace classmngr::engine
