#include "classmngr/engine/sub_prep_package.h"

#include "classmngr/engine/class_naming.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <set>
#include <string_view>
#include <utility>

namespace classmngr::engine
{
namespace
{
constexpr std::size_t MaximumPathComponentLength = 120;

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

std::string collapseAsciiWhitespace(std::string_view value)
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

std::string normalizeHyphens(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    std::size_t index = 0;
    while (index < value.size())
    {
        if (value[index] != '-')
        {
            result.push_back(value[index++]);
            continue;
        }

        while (!result.empty() && result.back() == ' ')
        {
            result.pop_back();
        }
        while (index < value.size()
               && (value[index] == '-' || value[index] == ' '))
        {
            ++index;
        }
        if (!result.empty())
        {
            result.push_back(' ');
        }
        result.push_back('-');
        if (index < value.size())
        {
            result.push_back(' ');
        }
    }
    return result;
}

std::string asciiUpper(std::string_view value)
{
    std::string result(value);
    for (char& character : result)
    {
        if (character >= 'a' && character <= 'z')
        {
            character = static_cast<char>(character - 'a' + 'A');
        }
    }
    return result;
}

std::string asciiCaseFold(std::string_view value)
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

std::size_t utf8CharacterWidth(
    const std::string& value,
    std::size_t index
    )
{
    const unsigned char first = static_cast<unsigned char>(value[index]);
    if (first < 0x80)
    {
        return 1;
    }
    if ((first & 0xE0) == 0xC0 && index + 1 < value.size())
    {
        return 2;
    }
    if ((first & 0xF0) == 0xE0 && index + 2 < value.size())
    {
        return 3;
    }
    if ((first & 0xF8) == 0xF0 && index + 3 < value.size())
    {
        return 4;
    }
    return 1;
}

std::string truncateUtf8(
    const std::string& value,
    std::size_t maximumBytes
    )
{
    if (value.size() <= maximumBytes)
    {
        return value;
    }

    std::size_t length = 0;
    while (length < value.size())
    {
        const std::size_t width = utf8CharacterWidth(value, length);
        if (length + width > maximumBytes)
        {
            break;
        }
        length += width;
    }
    return value.substr(0, length);
}

bool contains(
    const std::vector<std::string>& values,
    std::string_view value
    )
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

std::vector<CalendarDate> normalizedDates(
    const std::vector<CalendarDate>& dates
    )
{
    std::vector<CalendarDate> result;
    for (const CalendarDate& date : dates)
    {
        if (date.ok()
            && std::find(result.begin(), result.end(), date) == result.end())
        {
            result.push_back(date);
        }
    }
    std::sort(
        result.begin(),
        result.end(),
        [](const CalendarDate& left, const CalendarDate& right)
        {
            return std::chrono::sys_days{left}
                < std::chrono::sys_days{right};
        }
        );
    return result;
}

std::string twoDigits(unsigned value)
{
    std::string result = std::to_string(value);
    if (result.size() == 1)
    {
        result.insert(result.begin(), '0');
    }
    return result;
}

std::string dateText(
    const CalendarDate& date,
    bool includeYear
    )
{
    constexpr std::array<std::string_view, 12> months{
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    const unsigned month = static_cast<unsigned>(date.month());
    std::string result = twoDigits(static_cast<unsigned>(date.day()));
    result += ' ';
    result += months.at(month - 1);
    if (includeYear)
    {
        result += ' ';
        result += std::to_string(static_cast<int>(date.year()));
    }
    return result;
}

std::string uniqueFolderName(
    std::string_view preferred,
    std::set<std::string>* usedNames
    )
{
    const std::string base =
        SubPrepPackageService::safePathComponent(preferred, "Class");
    std::string candidate = base;
    int suffix = 2;

    while (usedNames && usedNames->contains(asciiCaseFold(candidate)))
    {
        const std::string suffixText = " (" + std::to_string(suffix++) + ')';
        const std::size_t prefixLength = suffixText.size()
            >= MaximumPathComponentLength
            ? 0
            : MaximumPathComponentLength - suffixText.size();
        candidate = truncateUtf8(base, prefixLength) + suffixText;
    }

    if (usedNames)
    {
        usedNames->insert(asciiCaseFold(candidate));
    }
    return candidate;
}

std::string weekdayName(const CalendarDate& date)
{
    switch (std::chrono::weekday{std::chrono::sys_days{date}}.iso_encoding())
    {
    case 1:
        return "Monday";
    case 2:
        return "Tuesday";
    case 3:
        return "Wednesday";
    case 4:
        return "Thursday";
    case 5:
        return "Friday";
    case 6:
        return "Saturday";
    case 7:
        return "Sunday";
    default:
        return {};
    }
}

std::string templateName(SubPrepRosterTemplate rosterTemplate)
{
    switch (rosterTemplate)
    {
    case SubPrepRosterTemplate::ByDay:
        return "By Day";
    case SubPrepRosterTemplate::Daily:
        return "Daily";
    case SubPrepRosterTemplate::PerClassWithExtraInfo:
        return "Per Class";
    }
    return "By Day";
}
} // namespace

std::string SubPrepPackageService::safePathComponent(
    std::string_view value,
    std::string_view fallback
    )
{
    std::string result = trimAsciiWhitespace(value);
    std::string bulletReplaced;
    bulletReplaced.reserve(result.size());
    for (std::size_t index = 0; index < result.size();)
    {
        if (index + 3 <= result.size()
            && result[index] == '\xE2'
            && result[index + 1] == '\x80'
            && result[index + 2] == '\xA2')
        {
            bulletReplaced += " - ";
            index += 3;
        }
        else
        {
            bulletReplaced.push_back(result[index++]);
        }
    }
    result = std::move(bulletReplaced);

    constexpr std::string_view invalid = "<>\"/\\|?*";
    for (char& character : result)
    {
        if (character == ':')
        {
            character = '.';
        }
        else if (invalid.find(character) != std::string_view::npos
                 || static_cast<unsigned char>(character) < 32)
        {
            character = '-';
        }
    }

    result = normalizeHyphens(collapseAsciiWhitespace(result));
    while (!result.empty()
           && (result.back() == '.' || result.back() == ' '))
    {
        result.pop_back();
    }

    if (result.empty())
    {
        result = trimAsciiWhitespace(fallback);
    }
    if (result.empty())
    {
        result = "Sub Prep";
    }

    constexpr std::array<std::string_view, 22> reservedNames{
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7",
        "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5",
        "LPT6", "LPT7", "LPT8", "LPT9"
    };
    const std::string uppercase = asciiUpper(result);
    if (std::find(reservedNames.begin(), reservedNames.end(), uppercase)
        != reservedNames.end())
    {
        result.insert(result.begin(), '_');
    }

    if (result.size() > MaximumPathComponentLength)
    {
        result = truncateUtf8(result, MaximumPathComponentLength);
        while (!result.empty() && result.back() == '.')
        {
            result.pop_back();
        }
    }
    return result;
}

std::string SubPrepPackageService::datedFolderName(
    std::string_view userName,
    const std::vector<CalendarDate>& selectedDates
    )
{
    const std::vector<CalendarDate> dates = normalizedDates(selectedDates);
    if (dates.empty())
    {
        return {};
    }

    const CalendarDate& first = dates.front();
    const CalendarDate& last = dates.back();
    std::string datePart;
    if (first == last)
    {
        datePart = dateText(first, true);
    }
    else if (first.year() != last.year())
    {
        datePart = dateText(first, true) + " - " + dateText(last, true);
    }
    else if (first.month() != last.month())
    {
        datePart = dateText(first, false) + " - " + dateText(last, true);
    }
    else
    {
        datePart = twoDigits(static_cast<unsigned>(first.day()))
            + " - " + dateText(last, true);
    }

    return safePathComponent(userName, "Sub Prep") + " (" + datePart + ')';
}

std::vector<std::string> SubPrepPackageService::selectedDayNames(
    const std::vector<CalendarDate>& selectedDates
    )
{
    std::vector<std::string> result;
    for (const CalendarDate& date : normalizedDates(selectedDates))
    {
        const std::string day = weekdayName(date);
        if (!day.empty() && !contains(result, day))
        {
            result.push_back(day);
        }
    }
    return result;
}

std::vector<int> SubPrepPackageService::classIdsForDays(
    const std::vector<SubPrepScheduleCell>& schedule,
    const std::vector<std::string>& selectedDays
    )
{
    std::vector<int> result;
    for (const SubPrepScheduleCell& cell : schedule)
    {
        if (!contains(selectedDays, cell.day))
        {
            continue;
        }
        for (const int classId : cell.classIds)
        {
            if (classId > 0
                && std::find(result.begin(), result.end(), classId)
                    == result.end())
            {
                result.push_back(classId);
            }
        }
    }
    return result;
}

std::string SubPrepPackageService::rosterDocumentFileName(
    SubPrepRosterTemplate rosterTemplate
    )
{
    if (rosterTemplate == SubPrepRosterTemplate::PerClassWithExtraInfo)
    {
        return "Roster.pdf";
    }
    return "Rosters - " + templateName(rosterTemplate) + ".pdf";
}

Result<SubPrepPackagePlan> SubPrepPackageService::build(
    const std::vector<SubPrepPackageSourceClass>& sourceClasses,
    const SubPrepPackageBuildOptions& options
    )
{
    const std::vector<CalendarDate> dates = normalizedDates(
        options.selectedDates
        );
    if (dates.empty())
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "At least one valid date is required for a Sub Prep package."
            ));
    }
    if (options.classIds.empty())
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "No classes were selected for the Sub Prep package."
            ));
    }

    const std::vector<std::string> days = selectedDayNames(dates);
    std::set<int> seenClassIds;
    std::vector<SubPrepPackageClass> classes;

    for (const int classId : options.classIds)
    {
        if (classId <= 0 || !seenClassIds.insert(classId).second)
        {
            continue;
        }

        const auto source = std::find_if(
            sourceClasses.begin(),
            sourceClasses.end(),
            [classId](const SubPrepPackageSourceClass& candidate)
            {
                return candidate.classroom.id == classId;
            }
            );
        if (source == sourceClasses.end())
        {
            return std::unexpected(error(
                ErrorCode::NotFound,
                "No source class exists for id " + std::to_string(classId)
                    + "."
                ));
        }

        const std::vector<ClassTime>& sourceTimes = options.useIntensiveSchedule
            ? source->info.intensiveTimes
            : source->info.classTimes;
        std::vector<ClassTime> filteredTimes;
        for (const ClassTime& time : sourceTimes)
        {
            if (contains(days, trimAsciiWhitespace(time.day)))
            {
                filteredTimes.push_back(time);
            }
        }
        if (filteredTimes.empty())
        {
            continue;
        }

        SubPrepPackageClass packageClass;
        packageClass.classroom = source->classroom;
        packageClass.info = source->info;
        packageClass.info.classId = classId;
        packageClass.info.classTimes = std::move(filteredTimes);
        packageClass.teacher = source->teacher;
        packageClass.displayName = ClassNamingService::classDisplayName(
            packageClass.info,
            packageClass.teacher
            );
        classes.push_back(std::move(packageClass));
    }

    std::sort(
        classes.begin(),
        classes.end(),
        [](const SubPrepPackageClass& left, const SubPrepPackageClass& right)
        {
            if (left.displayName != right.displayName)
            {
                return left.displayName < right.displayName;
            }
            return left.classroom.id < right.classroom.id;
        }
        );

    std::set<std::string> usedNames;
    for (SubPrepPackageClass& packageClass : classes)
    {
        packageClass.folderName = uniqueFolderName(
            packageClass.displayName,
            &usedNames
            );
    }

    if (classes.empty())
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "No classes meet on the selected days."
            ));
    }

    SubPrepPackagePlan plan;
    plan.folderName = datedFolderName(options.userName, dates);
    plan.relativeDocumentPaths.push_back("Sub Prep.pdf");
    if (options.rosterTemplate == SubPrepRosterTemplate::PerClassWithExtraInfo)
    {
        for (const SubPrepPackageClass& packageClass : classes)
        {
            plan.relativeDocumentPaths.push_back(
                packageClass.folderName + "/Roster.pdf"
                );
        }
    }
    else
    {
        plan.relativeDocumentPaths.push_back(
            rosterDocumentFileName(options.rosterTemplate)
            );
    }
    plan.classes = std::move(classes);
    return plan;
}

} // namespace classmngr::engine
