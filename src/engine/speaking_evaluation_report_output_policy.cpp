#include "classmngr/engine/speaking_evaluation_report_output_policy.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace classmngr::engine
{
namespace
{
std::string trimAsciiWhitespace(std::string_view value)
{
    std::size_t first = 0;
    while (
        first < value.size()
        && std::isspace(static_cast<unsigned char>(value[first]))
        )
    {
        ++first;
    }

    std::size_t last = value.size();
    while (
        last > first
        && std::isspace(static_cast<unsigned char>(value[last - 1]))
        )
    {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

std::string simplifiedAsciiWhitespace(std::string_view value)
{
    const std::string trimmed = trimAsciiWhitespace(value);
    std::string result;
    result.reserve(trimmed.size());

    bool pendingSpace = false;
    for (const unsigned char character : trimmed)
    {
        if (std::isspace(character))
        {
            pendingSpace = true;
            continue;
        }

        if (pendingSpace && !result.empty())
        {
            result.push_back(' ');
        }
        pendingSpace = false;
        result.push_back(static_cast<char>(character));
    }

    return result;
}

std::string safeFolderName(
    std::string_view value,
    std::string_view fallback
    )
{
    std::string name = trimAsciiWhitespace(value);
    for (char& character : name)
    {
        switch (character)
        {
        case '\\':
        case '/':
        case ':':
        case '*':
        case '?':
        case '"':
        case '<':
        case '>':
        case '|':
            character = '-';
            break;
        default:
            break;
        }
    }

    name = simplifiedAsciiWhitespace(name);
    return name.empty() ? std::string(fallback) : name;
}

std::string shortDay(std::string_view day)
{
    const std::string normalized = simplifiedAsciiWhitespace(day);
    const auto startsWith = [&normalized](std::string_view prefix)
    {
        if (normalized.size() < prefix.size())
        {
            return false;
        }

        for (std::size_t index = 0; index < prefix.size(); ++index)
        {
            if (std::tolower(
                    static_cast<unsigned char>(normalized[index])
                    )
                != std::tolower(
                    static_cast<unsigned char>(prefix[index])
                    ))
            {
                return false;
            }
        }
        return true;
    };

    if (startsWith("mon")) return "M";
    if (startsWith("tue")) return "T";
    if (startsWith("wed")) return "W";
    if (startsWith("thu")) return "Th";
    if (startsWith("fri")) return "F";
    if (startsWith("sat")) return "Sa";
    if (startsWith("sun")) return "Su";

    const std::string trimmed = trimAsciiWhitespace(day);
    return trimmed.substr(0, std::min<std::size_t>(2, trimmed.size()));
}

bool equalsIgnoreAsciiCase(
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
        if (std::tolower(static_cast<unsigned char>(left[index]))
            != std::tolower(static_cast<unsigned char>(right[index])))
        {
            return false;
        }
    }

    return true;
}

bool parseUnsigned(
    std::string_view value,
    unsigned* result
    )
{
    if (!result || value.empty())
    {
        return false;
    }

    unsigned parsed = 0;
    for (const unsigned char character : value)
    {
        if (character < '0' || character > '9')
        {
            return false;
        }
        parsed = (parsed * 10U) + (character - '0');
    }

    *result = parsed;
    return true;
}

std::string shortTime(std::string_view value)
{
    const std::string trimmed = trimAsciiWhitespace(value);
    const std::size_t colon = trimmed.find(':');
    if (colon != std::string::npos)
    {
        const std::string hourText = trimmed.substr(0, colon);
        std::size_t minuteEnd = trimmed.size();
        std::string suffix;

        while (minuteEnd > colon + 1
               && std::isspace(
                   static_cast<unsigned char>(trimmed[minuteEnd - 1])
                   ))
        {
            --minuteEnd;
        }

        if (minuteEnd >= 2)
        {
            const std::string candidate = trimmed.substr(minuteEnd - 2, 2);
            if (equalsIgnoreAsciiCase(candidate, "AM")
                || equalsIgnoreAsciiCase(candidate, "PM"))
            {
                suffix = candidate;
                minuteEnd -= 2;
                while (minuteEnd > colon + 1
                       && std::isspace(
                           static_cast<unsigned char>(trimmed[minuteEnd - 1])
                           ))
                {
                    --minuteEnd;
                }
            }
        }

        unsigned hour = 0;
        unsigned minute = 0;
        if (parseUnsigned(hourText, &hour)
            && parseUnsigned(
                std::string_view(trimmed).substr(
                    colon + 1,
                    minuteEnd - (colon + 1)
                    ),
                &minute
                )
            && minute <= 59)
        {
            if (!suffix.empty())
            {
                if (hour < 1 || hour > 12)
                {
                    return {};
                }
                if (equalsIgnoreAsciiCase(suffix, "AM"))
                {
                    if (hour == 12) hour = 0;
                }
                else if (hour != 12)
                {
                    hour += 12;
                }
            }
            else if (hour > 23)
            {
                return {};
            }

            const unsigned displayHour =
                hour % 12 == 0 ? 12 : hour % 12;
            std::string result = std::to_string(displayHour);
            if (minute != 0)
            {
                result += ':';
                if (minute < 10) result += '0';
                result += std::to_string(minute);
            }
            result += hour < 12 ? "am" : "pm";
            return result;
        }
    }

    std::string fallback = trimmed;
    fallback.erase(
        std::remove(fallback.begin(), fallback.end(), ' '),
        fallback.end()
        );
    std::ranges::transform(
        fallback,
        fallback.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        }
        );
    return fallback;
}

std::string cleanPath(std::string_view path)
{
    std::string result(path);
    std::ranges::replace(result, '\\', '/');

    std::string compact;
    compact.reserve(result.size());
    bool previousSlash = false;
    for (const char character : result)
    {
        if (character == '/')
        {
            if (previousSlash)
            {
                continue;
            }
            previousSlash = true;
        }
        else
        {
            previousSlash = false;
        }
        compact.push_back(character);
    }

    while (compact.size() > 1 && compact.back() == '/')
    {
        compact.pop_back();
    }
    return compact;
}

std::string joinPath(
    std::string_view directory,
    std::string_view child
    )
{
    const std::string cleanDirectory = cleanPath(directory);
    if (cleanDirectory.empty())
    {
        return cleanPath(child);
    }
    if (child.empty())
    {
        return cleanDirectory;
    }
    return cleanDirectory + '/' + cleanPath(child);
}

std::string lastPathComponent(std::string_view path)
{
    const std::string clean = cleanPath(path);
    const std::size_t slash = clean.find_last_of('/');
    return slash == std::string::npos ? clean : clean.substr(slash + 1);
}

bool endsWithIgnoreAsciiCase(
    std::string_view value,
    std::string_view suffix
    )
{
    if (value.size() < suffix.size())
    {
        return false;
    }

    const std::size_t offset = value.size() - suffix.size();
    for (std::size_t index = 0; index < suffix.size(); ++index)
    {
        if (std::tolower(
                static_cast<unsigned char>(value[offset + index])
                )
            != std::tolower(
                static_cast<unsigned char>(suffix[index])
                ))
        {
            return false;
        }
    }
    return true;
}

std::string normalizedExtension(std::string_view extension)
{
    std::string result = trimAsciiWhitespace(extension);
    if (!result.empty() && result.front() != '.')
    {
        result.insert(result.begin(), '.');
    }
    return result;
}

std::string sanitizedFileName(
    std::string_view value,
    char replacement
    )
{
    std::string result = trimAsciiWhitespace(value);
    for (char& character : result)
    {
        const unsigned char code = static_cast<unsigned char>(character);
        if (code < 0x20
            || code == 0x7f
            || character == '\\'
            || character == '/'
            || character == ':'
            || character == '*'
            || character == '?'
            || character == '"'
            || character == '<'
            || character == '>'
            || character == '|')
        {
            character = replacement;
        }
    }

    while (!result.empty()
           && (result.back() == ' ' || result.back() == '.'))
    {
        result.pop_back();
    }
    return result;
}

bool isWindowsReservedFileName(std::string_view value)
{
    const std::size_t dot = value.find('.');
    const std::string stem = std::string(value.substr(0, dot));
    static constexpr std::string_view reservedNames[] = {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7",
        "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5",
        "LPT6", "LPT7", "LPT8", "LPT9"
    };

    for (const std::string_view reserved : reservedNames)
    {
        if (equalsIgnoreAsciiCase(stem, reserved))
        {
            return true;
        }
    }
    return false;
}

std::string limitUtf8Bytes(
    std::string_view value,
    std::size_t maximumBytes
    )
{
    if (value.size() <= maximumBytes)
    {
        return std::string(value);
    }

    std::size_t boundary = 0;
    while (boundary < value.size())
    {
        const unsigned char lead =
            static_cast<unsigned char>(value[boundary]);
        std::size_t width = 1;
        if ((lead & 0x80U) == 0)
        {
            width = 1;
        }
        else if ((lead & 0xE0U) == 0xC0U)
        {
            width = 2;
        }
        else if ((lead & 0xF0U) == 0xE0U)
        {
            width = 3;
        }
        else if ((lead & 0xF8U) == 0xF0U)
        {
            width = 4;
        }

        if (boundary + width > maximumBytes
            || boundary + width > value.size())
        {
            break;
        }
        boundary += width;
    }

    return std::string(value.substr(0, boundary));
}
} // namespace

std::string SpeakingEvaluationReportOutputPolicy::defaultDirectory(
    const ClassInfo& classInfo,
    std::string_view evaluationName,
    std::string_view documentsDirectory,
    std::string_view classFallback,
    std::string_view evaluationFallback
    )
{
    std::string className = simplifiedAsciiWhitespace(classInfo.classGrade);
    const std::string level = simplifiedAsciiWhitespace(classInfo.classLevel);
    if (!className.empty() && !level.empty())
    {
        className += ' ';
    }
    className += level;
    if (className.empty())
    {
        className = std::string(classFallback);
    }

    std::vector<std::string> days;
    for (const ClassTime& classTime : classInfo.classTimes)
    {
        const std::string day = shortDay(classTime.day);
        if (!day.empty()
            && std::ranges::find(days, day) == days.end())
        {
            days.push_back(day);
        }
    }

    if (!classInfo.classTimes.empty())
    {
        const std::string time = shortTime(
            classInfo.classTimes.front().startTime
            );
        std::string schedule;
        if (!days.empty() && !time.empty())
        {
            schedule = std::string();
            for (const std::string& day : days)
            {
                schedule += day;
            }
            schedule += " - " + time;
        }
        else if (!days.empty())
        {
            for (const std::string& day : days)
            {
                schedule += day;
            }
        }
        else
        {
            schedule = time;
        }

        if (!schedule.empty())
        {
            className += " (" + schedule + ')';
        }
    }

    return joinPath(
        documentsDirectory,
        "DYB/SpeakingEvals/"
            + safeFolderName(className, classFallback)
            + '/'
            + safeFolderName(evaluationName, evaluationFallback)
        );
}

std::string SpeakingEvaluationReportOutputPolicy::batchArchivePath(
    std::string_view outputDirectory,
    std::string_view fallbackName
    )
{
    const std::string directory = cleanPath(outputDirectory);
    const std::string baseName = safeFolderName(
        lastPathComponent(directory),
        fallbackName
        );
    return joinPath(directory, baseName + ".zip");
}

std::string SpeakingEvaluationReportOutputPolicy::studentFileName(
    std::string_view englishName,
    std::string_view koreanName,
    std::string_view extension,
    std::string_view fallbackName,
    char replacement
    )
{
    const std::string english = trimAsciiWhitespace(englishName);
    const std::string korean = trimAsciiWhitespace(koreanName);
    std::string baseName;
    if (!english.empty() && !korean.empty())
    {
        baseName = english + " (" + korean + ')';
    }
    else
    {
        baseName = !english.empty() ? english : korean;
    }

    const std::string suffix = normalizedExtension(extension);
    while (!suffix.empty() && endsWithIgnoreAsciiCase(baseName, suffix))
    {
        baseName.resize(baseName.size() - suffix.size());
    }

    baseName = sanitizedFileName(
        simplifiedAsciiWhitespace(baseName),
        replacement
        );
    if (baseName.empty())
    {
        baseName = sanitizedFileName(
            simplifiedAsciiWhitespace(fallbackName),
            replacement
            );
    }
    if (baseName.empty())
    {
        baseName = "Document";
    }
    if (isWindowsReservedFileName(baseName))
    {
        baseName.insert(baseName.begin(), '_');
    }

    return limitUtf8Bytes(baseName, 240) + suffix;
}

} // namespace classmngr::engine
