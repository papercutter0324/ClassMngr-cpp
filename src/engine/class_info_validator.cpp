#include "classmngr/engine/class_info_validator.h"

#include "classmngr/engine/class_info_config.h"
#include "classmngr/engine/class_time_validator.h"

#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace classmngr::engine::ClassInfoValidator
{
namespace
{
constexpr std::size_t NotesMaximumLength = 10000;

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

bool contains(
    const ClassInfoConfig::StringList& values,
    std::string_view value
    )
{
    for (const std::string& candidate : values)
    {
        if (candidate == value)
        {
            return true;
        }
    }
    return false;
}

std::string canonicalChoice(
    std::string_view value,
    const ClassInfoConfig::StringList& choices
    )
{
    const std::string trimmed = trimAsciiWhitespace(value);
    for (const std::string& choice : choices)
    {
        if (equalsAsciiInsensitive(trimmed, choice))
        {
            return choice;
        }
    }
    return trimmed;
}

std::optional<std::string> canonicalHexColor(std::string_view value)
{
    const std::string trimmed = trimAsciiWhitespace(value);
    if (trimmed.size() != 7 || trimmed[0] != '#')
    {
        return std::nullopt;
    }
    for (std::size_t index = 1; index < trimmed.size(); ++index)
    {
        const char character = trimmed[index];
        const bool digit = character >= '0' && character <= '9';
        const bool lowerHex = character >= 'a' && character <= 'f';
        const bool upperHex = character >= 'A' && character <= 'F';
        if (!digit && !lowerHex && !upperHex)
        {
            return std::nullopt;
        }
    }

    std::string result = trimmed;
    for (char& character : result)
    {
        character = upperAscii(character);
    }
    return result;
}

void addIssue(
    ValidationResult& result,
    std::string_view code,
    std::string field
    )
{
    result.add(ValidationIssue{
        std::string(code),
        std::move(field),
        ValidationSeverity::Error
    });
}

void addAllowedValueIssue(
    ValidationResult& result,
    std::string_view field
    )
{
    addIssue(result, "class_info.value.not_allowed", std::string(field));
}

std::size_t utf16Length(std::string_view value)
{
    std::size_t length = 0;
    for (std::size_t index = 0; index < value.size();)
    {
        const unsigned char first = static_cast<unsigned char>(value[index]);
        std::uint32_t codePoint = 0xFFFD;
        std::size_t width = 1;
        if (first < 0x80)
        {
            codePoint = first;
        }
        else if ((first & 0xE0) == 0xC0 && index + 1 < value.size())
        {
            const unsigned char second = static_cast<unsigned char>(value[index + 1]);
            if ((second & 0xC0) == 0x80)
            {
                codePoint = ((first & 0x1F) << 6) | (second & 0x3F);
                width = 2;
            }
        }
        else if ((first & 0xF0) == 0xE0 && index + 2 < value.size())
        {
            const unsigned char second = static_cast<unsigned char>(value[index + 1]);
            const unsigned char third = static_cast<unsigned char>(value[index + 2]);
            if ((second & 0xC0) == 0x80 && (third & 0xC0) == 0x80)
            {
                codePoint = ((first & 0x0F) << 12)
                    | ((second & 0x3F) << 6)
                    | (third & 0x3F);
                width = 3;
            }
        }
        else if ((first & 0xF8) == 0xF0 && index + 3 < value.size())
        {
            const unsigned char second = static_cast<unsigned char>(value[index + 1]);
            const unsigned char third = static_cast<unsigned char>(value[index + 2]);
            const unsigned char fourth = static_cast<unsigned char>(value[index + 3]);
            if ((second & 0xC0) == 0x80
                && (third & 0xC0) == 0x80
                && (fourth & 0xC0) == 0x80)
            {
                codePoint = ((first & 0x07) << 18)
                    | ((second & 0x3F) << 12)
                    | ((third & 0x3F) << 6)
                    | (fourth & 0x3F);
                width = 4;
            }
        }

        if (codePoint > 0xFFFF)
        {
            length += 2;
        }
        else
        {
            ++length;
        }
        index += width;
    }
    return length;
}

void validateTextLength(
    ValidationResult& result,
    std::string_view value,
    std::string_view field
    )
{
    if (utf16Length(value) > NotesMaximumLength)
    {
        addIssue(result, "validation.length.out_of_bounds", std::string(field));
    }
}
} // namespace

ClassInfo normalized(const ClassInfo& info)
{
    ClassInfo result = info;
    result.classGrade = canonicalChoice(info.classGrade, ClassInfoConfig::grades());
    const ClassInfoConfig::StringList levels =
        ClassInfoConfig::levelsForGrade(result.classGrade);
    result.classLevel = canonicalChoice(info.classLevel, levels);

    if (contains(ClassInfoConfig::grades(), result.classGrade)
        && contains(levels, result.classLevel))
    {
        result.readingBook = canonicalChoice(
            info.readingBook,
            ClassInfoConfig::readingBooks(result.classGrade, result.classLevel)
            );
        result.essayBook = canonicalChoice(
            info.essayBook,
            ClassInfoConfig::essayBooks(result.classGrade, result.classLevel)
            );
    }
    else
    {
        result.readingBook = trimAsciiWhitespace(info.readingBook);
        result.essayBook = trimAsciiWhitespace(info.essayBook);
    }

    for (ClassTime& time : result.classTimes)
    {
        time = ClassTimeValidator::normalized(time);
    }
    for (ClassTime& time : result.intensiveTimes)
    {
        time = ClassTimeValidator::normalized(time);
    }

    result.notes = trimAsciiWhitespace(info.notes);
    result.timeFillerActivities = trimAsciiWhitespace(info.timeFillerActivities);

    result.classColor = canonicalHexColor(info.classColor).value_or(
        trimAsciiWhitespace(info.classColor)
        );
    result.fontColor = canonicalHexColor(info.fontColor).value_or(
        trimAsciiWhitespace(info.fontColor)
        );
    return result;
}

ValidationResult validate(const ClassInfo& info)
{
    ValidationResult result;
    if (info.classId <= 0)
    {
        addIssue(result, "class_info.class_id.invalid", "classId");
    }
    if (info.teacherId == 0 || info.teacherId < -1)
    {
        addIssue(result, "class_info.teacher_id.invalid", "teacherId");
    }

    const std::string grade = trimAsciiWhitespace(info.classGrade);
    const std::string level = trimAsciiWhitespace(info.classLevel);
    if (grade.empty() != level.empty())
    {
        addIssue(
            result,
            grade.empty() ? "class_info.grade.required" : "class_info.level.required",
            grade.empty() ? "classGrade" : "classLevel"
            );
    }
    else if (!grade.empty())
    {
        if (!contains(ClassInfoConfig::grades(), grade))
        {
            addAllowedValueIssue(result, "classGrade");
        }

        const ClassInfoConfig::StringList levels =
            ClassInfoConfig::levelsForGrade(grade);
        if (!contains(levels, level))
        {
            addAllowedValueIssue(result, "classLevel");
        }

        if (contains(ClassInfoConfig::grades(), grade)
            && contains(levels, level))
        {
            if (!contains(
                    ClassInfoConfig::readingBooks(grade, level),
                    trimAsciiWhitespace(info.readingBook)))
            {
                addAllowedValueIssue(result, "readingBook");
            }
            if (!contains(
                    ClassInfoConfig::essayBooks(grade, level),
                    trimAsciiWhitespace(info.essayBook)))
            {
                addAllowedValueIssue(result, "essayBook");
            }
        }
    }
    else
    {
        if (!trimAsciiWhitespace(info.readingBook).empty())
        {
            addIssue(result, "class_info.book.requires_grade_level", "readingBook");
        }
        if (!trimAsciiWhitespace(info.essayBook).empty())
        {
            addIssue(result, "class_info.book.requires_grade_level", "essayBook");
        }
    }

    if (!canonicalHexColor(info.classColor))
    {
        addIssue(result, "color.invalid_hex", "classColor");
    }
    if (!canonicalHexColor(info.fontColor))
    {
        addIssue(result, "color.invalid_hex", "fontColor");
    }

    result.merge(validateNotes(
        info.classId,
        info.notes,
        info.timeFillerActivities
        ));
    result.merge(ClassTimeValidator::validate(info.classTimes, "classTimes"));
    result.merge(ClassTimeValidator::validate(info.intensiveTimes, "intensiveTimes"));
    return result;
}

ValidationResult validateNotes(
    int classId,
    std::string_view notes,
    std::string_view timeFillerActivities
    )
{
    ValidationResult result;
    if (classId <= 0)
    {
        addIssue(result, "class_info.class_id.invalid", "classId");
    }
    validateTextLength(result, notes, "notes");
    validateTextLength(result, timeFillerActivities, "timeFillerActivities");
    return result;
}

} // namespace classmngr::engine::ClassInfoValidator
