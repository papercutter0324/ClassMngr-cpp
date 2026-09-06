#include "classmngr/engine/teacher_validator.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace classmngr::engine
{
namespace
{
constexpr std::size_t RoomNumberMaximumLength = 64;
constexpr std::size_t BirthdayMaximumLength = 5;
constexpr std::size_t PhoneNumberMaximumLength = 32;
constexpr std::size_t CredentialMaximumLength = 128;
constexpr std::size_t PasswordMaximumLength = 256;
constexpr std::size_t NotesMaximumLength = 10000;

constexpr std::array<std::string_view, 4> InternetTypes{
    "WiFi",
    "LAN",
    "Both",
    "N/A"
};

constexpr std::array<std::string_view, 4> ProjectionTypes{
    "HDMI",
    "Zoom",
    "Any",
    "N/A"
};

struct Utf8CodePoints
{
    std::vector<std::uint32_t> values;
    bool valid = true;
};

bool isAsciiWhitespace(
    char character
    ) noexcept
{
    return std::isspace(static_cast<unsigned char>(character)) != 0;
}

std::string trimAsciiWhitespace(
    std::string_view value
    )
{
    std::size_t first = 0;
    while (first < value.size() && isAsciiWhitespace(value[first]))
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first && isAsciiWhitespace(value[last - 1]))
    {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

Utf8CodePoints decodeUtf8(
    std::string_view value
    )
{
    Utf8CodePoints result;
    result.values.reserve(value.size());

    for (std::size_t index = 0; index < value.size();)
    {
        const auto byte = [value](std::size_t offset)
        {
            return static_cast<unsigned char>(value[offset]);
        };

        const unsigned char first = byte(index);
        std::uint32_t codePoint = 0;
        std::size_t length = 0;
        std::uint32_t minimum = 0;

        if (first <= 0x7F)
        {
            codePoint = first;
            length = 1;
            minimum = 0;
        }
        else if (first >= 0xC2 && first <= 0xDF)
        {
            codePoint = first & 0x1F;
            length = 2;
            minimum = 0x80;
        }
        else if (first >= 0xE0 && first <= 0xEF)
        {
            codePoint = first & 0x0F;
            length = 3;
            minimum = 0x800;
        }
        else if (first >= 0xF0 && first <= 0xF4)
        {
            codePoint = first & 0x07;
            length = 4;
            minimum = 0x10000;
        }
        else
        {
            result.valid = false;
            result.values.push_back(0xFFFD);
            ++index;
            continue;
        }

        if (index + length > value.size())
        {
            result.valid = false;
            result.values.push_back(0xFFFD);
            ++index;
            continue;
        }

        bool continuationBytes = true;
        for (std::size_t offset = 1; offset < length; ++offset)
        {
            if ((byte(index + offset) & 0xC0) != 0x80)
            {
                continuationBytes = false;
                break;
            }
            codePoint = (codePoint << 6) | (byte(index + offset) & 0x3F);
        }

        if (!continuationBytes
            || codePoint < minimum
            || codePoint > 0x10FFFF
            || (codePoint >= 0xD800 && codePoint <= 0xDFFF))
        {
            result.valid = false;
            result.values.push_back(0xFFFD);
            ++index;
            continue;
        }

        result.values.push_back(codePoint);
        index += length;
    }

    return result;
}

std::size_t utf8Length(
    std::string_view value
    )
{
    return decodeUtf8(value).values.size();
}

bool isAsciiLetter(
    char character
    ) noexcept
{
    return (character >= 'A' && character <= 'Z')
        || (character >= 'a' && character <= 'z');
}

bool hasNonAsciiBytes(
    std::string_view value
    ) noexcept
{
    return std::any_of(
        value.begin(),
        value.end(),
        [](char character)
        {
            return static_cast<unsigned char>(character) > 127;
        }
        );
}

bool containsInvalidEnglishCharacters(
    std::string_view value
    ) noexcept
{
    for (const char character : value)
    {
        if (isAsciiLetter(character)
            || character == '.'
            || character == '-'
            || isAsciiWhitespace(character)
            || static_cast<unsigned char>(character) > 127)
        {
            continue;
        }

        return true;
    }

    return false;
}

std::string collapseWhitespace(
    std::string_view value
    )
{
    std::string result;
    result.reserve(value.size());
    bool pendingSpace = false;

    for (const char character : value)
    {
        if (isAsciiWhitespace(character))
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

    return trimAsciiWhitespace(result);
}

void removeSpacesAround(
    std::string& value,
    char separator
    )
{
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        if (value[index] != separator)
        {
            continue;
        }

        while (index > 0 && value[index - 1] == ' ')
        {
            value.erase(index - 1, 1);
            --index;
        }

        while (index + 1 < value.size() && value[index + 1] == ' ')
        {
            value.erase(index + 1, 1);
        }
    }
}

void collapseRepeated(
    std::string& value,
    char character
    )
{
    value.erase(
        std::unique(
            value.begin(),
            value.end(),
            [character](char left, char right)
            {
                return left == character && right == character;
            }
            ),
        value.end()
        );
}

std::string lowerAscii(
    std::string value
    )
{
    for (char& character : value)
    {
        if (character >= 'A' && character <= 'Z')
        {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }

    return value;
}

bool equalInsensitive(
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
        const auto lower = [](char character)
        {
            return character >= 'A' && character <= 'Z'
                ? static_cast<char>(character - 'A' + 'a')
                : character;
        };
        if (lower(left[index]) != lower(right[index]))
        {
            return false;
        }
    }

    return true;
}

std::string normalizeEnglishName(
    std::string_view value
    )
{
    const std::string trimmed = trimAsciiWhitespace(value);
    if (trimmed.empty())
    {
        return {};
    }

    if (hasNonAsciiBytes(value) || containsInvalidEnglishCharacters(value))
    {
        return trimmed;
    }

    std::string cleaned = collapseWhitespace(value);
    removeSpacesAround(cleaned, '-');
    removeSpacesAround(cleaned, '.');
    collapseRepeated(cleaned, '-');
    collapseRepeated(cleaned, '.');

    static const std::regex joinedInitials(
        R"(\b([A-Za-z])[.-]+-?[.-]*([A-Za-z])\b)"
        );
    cleaned = std::regex_replace(cleaned, joinedInitials, "$1.$2");

    std::string result;
    std::string token;
    char previousSeparator = '\0';
    for (const char character : cleaned)
    {
        if (character == ' ' || character == '.' || character == '-')
        {
            if (!token.empty())
            {
                std::string normalizedToken = lowerAscii(std::move(token));
                if (result.empty()
                    || previousSeparator == ' '
                    || previousSeparator == '.')
                {
                    normalizedToken.front() = static_cast<char>(
                        normalizedToken.front() - 'a' + 'A'
                        );
                }
                result += normalizedToken;
                token.clear();
            }
            result.push_back(character);
            previousSeparator = character;
        }
        else
        {
            token.push_back(character);
        }
    }
    if (!token.empty())
    {
        std::string normalizedToken = lowerAscii(std::move(token));
        if (result.empty()
            || previousSeparator == ' '
            || previousSeparator == '.')
        {
            normalizedToken.front() = static_cast<char>(
                normalizedToken.front() - 'a' + 'A'
                );
        }
        result += normalizedToken;
    }

    static const std::regex adjacentInitials(
        R"(\b([A-Za-z])\. ?([A-Za-z])\.)"
        );
    result = std::regex_replace(result, adjacentInitials, "$1.$2.");
    return trimAsciiWhitespace(result);
}

bool isHangulSyllable(
    std::uint32_t codePoint
    ) noexcept
{
    return codePoint >= 0xAC00 && codePoint <= 0xD7A3;
}

bool isCodePointWhitespace(
    std::uint32_t codePoint
    ) noexcept
{
    return codePoint <= 0x7F
        && std::isspace(static_cast<unsigned char>(codePoint)) != 0;
}

struct KoreanNameParts
{
    std::vector<std::uint32_t> base;
    char suffix = '\0';
    bool valid = false;
};

KoreanNameParts parseKoreanName(
    std::string_view value
    )
{
    const std::string trimmed = trimAsciiWhitespace(value);
    const Utf8CodePoints decoded = decodeUtf8(trimmed);
    KoreanNameParts result;
    if (!decoded.valid || decoded.values.empty())
    {
        return result;
    }

    std::size_t baseEnd = decoded.values.size();
    if (baseEnd >= 3
        && decoded.values[baseEnd - 3] == '('
        && decoded.values[baseEnd - 1] == ')'
        && ((decoded.values[baseEnd - 2] >= 'A'
             && decoded.values[baseEnd - 2] <= 'Z')
            || (decoded.values[baseEnd - 2] >= 'a'
                && decoded.values[baseEnd - 2] <= 'z')))
    {
        result.suffix = static_cast<char>(decoded.values[baseEnd - 2]);
        if (result.suffix >= 'a' && result.suffix <= 'z')
        {
            result.suffix = static_cast<char>(result.suffix - 'a' + 'A');
        }
        baseEnd -= 3;
        while (baseEnd > 0 && isCodePointWhitespace(decoded.values[baseEnd - 1]))
        {
            --baseEnd;
        }
    }

    bool hasHangul = false;
    for (std::size_t index = 0; index < baseEnd; ++index)
    {
        const std::uint32_t codePoint = decoded.values[index];
        if (isHangulSyllable(codePoint))
        {
            hasHangul = true;
            result.base.push_back(codePoint);
        }
        else if (!isCodePointWhitespace(codePoint))
        {
            result.base.clear();
            return result;
        }
    }

    if (!hasHangul)
    {
        result.base.clear();
        return result;
    }

    result.valid = true;
    return result;
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

std::string normalizeKoreanName(
    std::string_view value
    )
{
    const KoreanNameParts parsed = parseKoreanName(value);
    if (!parsed.valid)
    {
        return trimAsciiWhitespace(value);
    }

    std::string result;
    result.reserve(value.size());
    for (const std::uint32_t codePoint : parsed.base)
    {
        appendUtf8(result, codePoint);
    }
    if (parsed.suffix != '\0')
    {
        result += '(';
        result += parsed.suffix;
        result += ')';
    }

    return result;
}

bool birthdayIsValid(
    std::string_view value
    ) noexcept
{
    const std::string birthday = trimAsciiWhitespace(value);
    if (birthday.empty())
    {
        return true;
    }
    if (birthday.size() != 5
        || birthday[2] != '-'
        || !std::isdigit(static_cast<unsigned char>(birthday[0]))
        || !std::isdigit(static_cast<unsigned char>(birthday[1]))
        || !std::isdigit(static_cast<unsigned char>(birthday[3]))
        || !std::isdigit(static_cast<unsigned char>(birthday[4])))
    {
        return false;
    }

    const int month = (birthday[0] - '0') * 10 + birthday[1] - '0';
    const int day = (birthday[3] - '0') * 10 + birthday[4] - '0';
    constexpr std::array<int, 12> days{
        31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    return month >= 1 && month <= 12 && day >= 1 && day <= days[month - 1];
}

ValidationIssue issue(
    std::string code,
    std::string field,
    ValidationSeverity severity = ValidationSeverity::Error
    )
{
    return {
        std::move(code),
        std::move(field),
        severity
    };
}

ValidationResult textLength(
    std::string_view value,
    std::size_t maximumLength,
    std::string_view fieldName
    )
{
    if (utf8Length(value) <= maximumLength)
    {
        return {};
    }

    return ValidationResult(issue(
        "validation.length.out_of_bounds",
        std::string(fieldName)
        ));
}

ValidationResult englishName(
    std::string_view value,
    std::string_view fieldName
    )
{
    ValidationResult result;
    if (utf8Length(value) > 20)
    {
        result.add(issue(
            "student_name.english.too_long",
            std::string(fieldName)
            ));
    }
    if (hasNonAsciiBytes(value))
    {
        result.add(issue(
            "student_name.english.non_ascii",
            std::string(fieldName)
            ));
    }
    if (containsInvalidEnglishCharacters(value))
    {
        result.add(issue(
            "student_name.english.invalid_characters",
            std::string(fieldName)
            ));
    }

    return result;
}

ValidationResult koreanName(
    std::string_view value,
    std::string_view fieldName
    )
{
    ValidationResult result;
    const KoreanNameParts parsed = parseKoreanName(value);
    if (!trimAsciiWhitespace(value).empty() && !parsed.valid)
    {
        result.add(issue(
            "student_name.korean.invalid_characters",
            std::string(fieldName)
            ));
    }

    const std::size_t length = parsed.base.size();
    if (length == 0 || length == 3)
    {
        return result;
    }
    if (length <= 1)
    {
        result.add(issue(
            "student_name.korean.too_short",
            std::string(fieldName)
            ));
    }
    else if (length >= 5)
    {
        result.add(issue(
            "student_name.korean.too_long",
            std::string(fieldName)
            ));
    }
    else
    {
        result.add(issue(
            "student_name.korean.unusual_length",
            std::string(fieldName),
            ValidationSeverity::Warning
            ));
    }

    return result;
}

std::string canonicalChoice(
    std::string_view value,
    const auto& choices
    )
{
    const std::string trimmed = trimAsciiWhitespace(value);
    for (const std::string_view choice : choices)
    {
        if (equalInsensitive(trimmed, choice))
        {
            return std::string(choice);
        }
    }

    return trimmed;
}

template<std::size_t N>
ValidationResult stringEnumValue(
    std::string_view value,
    const std::array<std::string_view, N>& allowed,
    std::string_view fieldName
    )
{
    if (std::find(allowed.begin(), allowed.end(), value) != allowed.end())
    {
        return {};
    }

    return ValidationResult(issue(
        "validation.enum.invalid_value",
        std::string(fieldName)
        ));
}
} // namespace

Teacher TeacherValidator::normalized(
    const Teacher& teacher
    )
{
    Teacher normalized = teacher;
    normalized.teacherKr = normalizeKoreanName(teacher.teacherKr);
    normalized.teacherEn = normalizeEnglishName(teacher.teacherEn);
    normalized.preferredRomanization = normalizeEnglishName(
        teacher.preferredRomanization
        );

    normalized.preferredName = canonicalChoice(
        teacher.preferredName,
        normalized.preferredNameChoices()
        );

    normalized.roomNumber = trimAsciiWhitespace(teacher.roomNumber);
    normalized.birthday = trimAsciiWhitespace(teacher.birthday);
    normalized.phoneNumber = normalizedPhoneNumber(teacher.phoneNumber);
    normalized.wifiName = trimAsciiWhitespace(teacher.wifiName);
    normalized.wifiPassword = trimAsciiWhitespace(teacher.wifiPassword);
    normalized.internetType = canonicalChoice(teacher.internetType, InternetTypes);
    normalized.zoomId = trimAsciiWhitespace(teacher.zoomId);
    normalized.zoomPassword = trimAsciiWhitespace(teacher.zoomPassword);
    normalized.projectionType = canonicalChoice(
        teacher.projectionType,
        ProjectionTypes
        );
    normalized.notes = trimAsciiWhitespace(teacher.notes);

    return normalized;
}

std::string TeacherValidator::normalizedPhoneNumber(
    std::string_view value
    )
{
    const std::string trimmed = trimAsciiWhitespace(value);
    if (trimmed.empty())
    {
        return {};
    }

    std::string digits;
    bool sawLeadingPlus = false;
    for (const char character : trimmed)
    {
        if (character >= '0' && character <= '9')
        {
            digits.push_back(character);
        }
        else if (character == '+' && digits.empty() && !sawLeadingPlus)
        {
            sawLeadingPlus = true;
        }
        else if (character != ' '
                 && character != '-'
                 && character != '('
                 && character != ')')
        {
            return trimmed;
        }
    }

    if (digits.empty())
    {
        return trimmed;
    }

    if (!sawLeadingPlus && digits.size() == 11
        && digits.starts_with("010"))
    {
        return digits.substr(0, 3) + "-"
            + digits.substr(3, 4) + "-"
            + digits.substr(7, 4);
    }

    if (!sawLeadingPlus && digits.size() == 10
        && digits.starts_with("02"))
    {
        return digits.substr(0, 2) + "-"
            + digits.substr(2, 4) + "-"
            + digits.substr(6, 4);
    }

    return sawLeadingPlus ? "+" + digits : digits;
}

ValidationResult TeacherValidator::validate(
    const Teacher& teacher
    )
{
    ValidationResult result;
    if (trimAsciiWhitespace(teacher.teacherKr).empty()
        && trimAsciiWhitespace(teacher.teacherEn).empty()
        && trimAsciiWhitespace(teacher.preferredRomanization).empty())
    {
        result.add(issue(
            "teacher.name.required",
            "teacherEn"
            ));
    }

    result.merge(englishName(teacher.teacherEn, "teacherEn"));
    result.merge(koreanName(teacher.teacherKr, "teacherKr"));
    result.merge(englishName(
        teacher.preferredRomanization,
        "preferredRomanization"
        ));

    const std::string preferredName = trimAsciiWhitespace(teacher.preferredName);
    const std::vector<std::string> choices = teacher.preferredNameChoices();
    if (!preferredName.empty()
        && std::find(choices.begin(), choices.end(), preferredName)
            == choices.end())
    {
        result.add(issue(
            "teacher.preferred_name.invalid_choice",
            "preferredName"
            ));
    }

    result.merge(textLength(
        teacher.roomNumber,
        RoomNumberMaximumLength,
        "roomNumber"
        ));
    result.merge(textLength(
        teacher.birthday,
        BirthdayMaximumLength,
        "birthday"
        ));
    result.merge(textLength(
        teacher.phoneNumber,
        PhoneNumberMaximumLength,
        "phoneNumber"
        ));
    result.merge(textLength(
        teacher.wifiName,
        CredentialMaximumLength,
        "wifiName"
        ));
    result.merge(textLength(
        teacher.wifiPassword,
        PasswordMaximumLength,
        "wifiPassword"
        ));
    result.merge(textLength(
        teacher.zoomId,
        CredentialMaximumLength,
        "zoomId"
        ));
    result.merge(textLength(
        teacher.zoomPassword,
        PasswordMaximumLength,
        "zoomPassword"
        ));
    result.merge(textLength(
        teacher.notes,
        NotesMaximumLength,
        "notes"
        ));

    if (!birthdayIsValid(teacher.birthday))
    {
        result.add(issue(
            "teacher.birthday.invalid",
            "birthday"
            ));
    }

    const std::string phone = trimAsciiWhitespace(teacher.phoneNumber);
    if (!phone.empty())
    {
        bool validCharacters = true;
        std::size_t digitCount = 0;
        for (std::size_t index = 0; index < phone.size(); ++index)
        {
            const char character = phone[index];
            if (character >= '0' && character <= '9')
            {
                ++digitCount;
            }
            else if (character == '+' && index == 0)
            {
                continue;
            }
            else if (character != ' '
                     && character != '-'
                     && character != '('
                     && character != ')')
            {
                validCharacters = false;
            }
        }

        if (!validCharacters || digitCount < 7 || digitCount > 15)
        {
            result.add(issue(
                "teacher.phone.invalid",
                "phoneNumber"
                ));
        }
    }

    result.merge(stringEnumValue(
        teacher.internetType,
        InternetTypes,
        "internetType"
        ));
    result.merge(stringEnumValue(
        teacher.projectionType,
        ProjectionTypes,
        "projectionType"
        ));

    return result;
}

} // namespace classmngr::engine
