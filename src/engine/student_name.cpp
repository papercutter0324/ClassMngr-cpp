#include "classmngr/engine/student_name.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <string>

namespace classmngr::engine
{
namespace
{
std::string trimAsciiWhitespace(std::string_view value)
{
    const auto isWhitespace = [](char character)
    {
        return std::isspace(static_cast<unsigned char>(character)) != 0;
    };

    std::size_t first = 0;
    while (first < value.size() && isWhitespace(value[first]))
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first && isWhitespace(value[last - 1]))
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

    for (const unsigned char character : value)
    {
        if (std::isspace(character) != 0)
        {
            pendingSpace = !result.empty();
            continue;
        }

        if (pendingSpace)
        {
            result.push_back(' ');
            pendingSpace = false;
        }
        result.push_back(static_cast<char>(character));
    }

    return result;
}

bool isAsciiLetter(char character)
{
    return (character >= 'A' && character <= 'Z')
        || (character >= 'a' && character <= 'z');
}

bool hasInvalidEnglishCharacter(std::string_view value)
{
    for (const unsigned char character : value)
    {
        if (isAsciiLetter(static_cast<char>(character))
            || character == '.'
            || character == '-'
            || std::isspace(character) != 0)
        {
            continue;
        }

        return true;
    }

    return false;
}

std::string asciiLower(std::string_view value)
{
    std::string result(value);
    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        }
        );
    return result;
}

std::string asciiUpper(char character)
{
    return std::string(1, static_cast<char>(std::toupper(
        static_cast<unsigned char>(character)
        )));
}

bool decodeUtf8(
    std::string_view value,
    std::size_t* offset,
    unsigned* codePoint
    )
{
    if (offset == nullptr || codePoint == nullptr || *offset >= value.size())
    {
        return false;
    }

    const auto byteAt = [&value](std::size_t index)
    {
        return static_cast<unsigned char>(value[index]);
    };

    const unsigned first = byteAt(*offset);
    if (first <= 0x7fU)
    {
        ++*offset;
        *codePoint = first;
        return true;
    }

    unsigned length = 0;
    unsigned result = 0;
    unsigned minimum = 0;
    if (first >= 0xc2U && first <= 0xdfU)
    {
        length = 2;
        result = first & 0x1fU;
        minimum = 0x80U;
    }
    else if (first >= 0xe0U && first <= 0xefU)
    {
        length = 3;
        result = first & 0x0fU;
        minimum = 0x800U;
    }
    else if (first >= 0xf0U && first <= 0xf4U)
    {
        length = 4;
        result = first & 0x07U;
        minimum = 0x10000U;
    }
    else
    {
        return false;
    }

    if (*offset + length > value.size())
    {
        return false;
    }

    for (unsigned index = 1; index < length; ++index)
    {
        const unsigned continuation = byteAt(*offset + index);
        if ((continuation & 0xc0U) != 0x80U)
        {
            return false;
        }
        result = (result << 6U) | (continuation & 0x3fU);
    }

    if (result < minimum || result > 0x10ffffU
        || (result >= 0xd800U && result <= 0xdfffU))
    {
        return false;
    }

    *offset += length;
    *codePoint = result;
    return true;
}

std::size_t koreanSuffixStart(std::string_view value)
{
    if (value.size() < 3)
    {
        return std::string_view::npos;
    }

    std::size_t end = value.size();
    while (end > 0
           && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
    {
        --end;
    }

    if (end < 3 || value[end - 3] != '(' || value[end - 1] != ')')
    {
        return std::string_view::npos;
    }

    const char suffix = value[end - 2];
    if (!isAsciiLetter(suffix))
    {
        return std::string_view::npos;
    }

    // Keep the index at the opening parenthesis.  Whitespace before the
    // suffix belongs to the source name and is discarded while the Hangul
    // code points are normalized.
    return end - 3;
}

bool isValidKoreanName(std::string_view value)
{
    if (value.empty())
    {
        return false;
    }

    const std::size_t suffixStart = koreanSuffixStart(value);
    bool hasHangul = false;
    std::size_t offset = 0;
    while (offset < value.size())
    {
        const std::size_t characterOffset = offset;
        unsigned codePoint = 0;
        if (!decodeUtf8(value, &offset, &codePoint))
        {
            return false;
        }

        if (codePoint <= 0x7fU)
        {
            if (std::isspace(static_cast<unsigned char>(codePoint)) != 0)
            {
                continue;
            }

            if (characterOffset == suffixStart
                && suffixStart != std::string_view::npos
                && value.size() - suffixStart == 3
                && value[suffixStart] == '('
                && value[suffixStart + 2] == ')'
                && isAsciiLetter(value[suffixStart + 1]))
            {
                // The suffix is ASCII and its exact shape was checked above;
                // consume its remaining bytes in the loop as ordinary ASCII.
                continue;
            }

            if (suffixStart != std::string_view::npos
                && characterOffset > suffixStart)
            {
                continue;
            }

            return false;
        }

        if (codePoint < 0xac00U || codePoint > 0xd7a3U)
        {
            return false;
        }
        hasHangul = true;
    }

    return hasHangul;
}

std::string normalizeKoreanSource(std::string_view value)
{
    const std::string trimmed = trimAsciiWhitespace(value);
    if (trimmed.empty() || !isValidKoreanName(trimmed))
    {
        return trimmed;
    }

    const std::size_t suffixStart = koreanSuffixStart(trimmed);
    const std::string_view source = suffixStart == std::string_view::npos
        ? std::string_view(trimmed)
        : std::string_view(trimmed).substr(0, suffixStart);

    std::string normalized;
    normalized.reserve(source.size());
    std::size_t offset = 0;
    while (offset < source.size())
    {
        unsigned codePoint = 0;
        if (!decodeUtf8(source, &offset, &codePoint))
        {
            return trimmed;
        }
        if (codePoint >= 0xac00U && codePoint <= 0xd7a3U)
        {
            // The source is already valid UTF-8, so the code-point range
            // check is enough to retain the original bytes.
            const std::size_t nextOffset = offset;
            std::size_t codePointStart = nextOffset;
            if (codePoint <= 0x7fU)
            {
                codePointStart = nextOffset - 1;
            }
            else if (codePoint <= 0x7ffU)
            {
                codePointStart = nextOffset - 2;
            }
            else if (codePoint <= 0xffffU)
            {
                codePointStart = nextOffset - 3;
            }
            else
            {
                codePointStart = nextOffset - 4;
            }
            normalized.append(source.substr(codePointStart, nextOffset - codePointStart));
        }
    }

    if (suffixStart != std::string_view::npos && !normalized.empty())
    {
        const char suffix = trimmed[suffixStart + 1];
        normalized.push_back('(');
        normalized.push_back(static_cast<char>(std::toupper(
            static_cast<unsigned char>(suffix)
            )));
        normalized.push_back(')');
    }

    return normalized;
}
} // namespace

std::string StudentNameService::normalizeEnglish(std::string_view value)
{
    const std::string trimmed = trimAsciiWhitespace(value);
    if (trimmed.empty())
    {
        return {};
    }

    // Preserve malformed input so validation in the retained Qt adapter does
    // not accidentally turn an invalid name into a valid one.
    if (hasInvalidEnglishCharacter(trimmed))
    {
        return trimmed;
    }

    std::string cleaned = collapseAsciiWhitespace(trimmed);
    cleaned = std::regex_replace(
        cleaned,
        std::regex(R"(\s*-\s*)"),
        "-"
        );
    cleaned = std::regex_replace(
        cleaned,
        std::regex(R"(-{2,})"),
        "-"
        );
    cleaned = std::regex_replace(
        cleaned,
        std::regex(R"(\.{2,})"),
        "."
        );
    cleaned = std::regex_replace(
        cleaned,
        std::regex(R"(\s*\.\s*)"),
        "."
        );
    cleaned = std::regex_replace(
        cleaned,
        std::regex(R"(\b([A-Za-z])[.-]+-?[.-]*([A-Za-z])\b)"),
        "$1.$2"
        );

    std::string result;
    std::string token;
    char previousSeparator = '\0';
    const auto flushToken = [&result, &token, &previousSeparator]()
    {
        if (token.empty())
        {
            return;
        }

        const std::string lower = asciiLower(token);
        if (result.empty() || previousSeparator == ' '
            || previousSeparator == '.')
        {
            result += asciiUpper(lower.front());
            result += lower.substr(1);
        }
        else
        {
            result += lower;
        }
        token.clear();
    };

    for (const char character : cleaned)
    {
        if (character == ' ' || character == '.' || character == '-')
        {
            flushToken();
            result.push_back(character);
            previousSeparator = character;
        }
        else
        {
            token.push_back(character);
        }
    }
    flushToken();

    cleaned = std::regex_replace(
        result,
        std::regex(R"(\b([A-Za-z])\. ?([A-Za-z])\.)"),
        "$1.$2."
        );
    return trimAsciiWhitespace(cleaned);
}

std::string StudentNameService::normalizeKorean(std::string_view value)
{
    return normalizeKoreanSource(value);
}

std::string StudentNameService::baseKorean(std::string_view value)
{
    std::string normalized = normalizeKorean(value);
    const std::size_t suffixStart = koreanSuffixStart(normalized);
    if (suffixStart != std::string_view::npos)
    {
        normalized.erase(suffixStart);
    }
    return normalized;
}

std::string StudentNameService::koreanSuffix(std::string_view value)
{
    const std::string normalized = normalizeKorean(value);
    const std::size_t suffixStart = koreanSuffixStart(normalized);
    if (suffixStart == std::string_view::npos)
    {
        return {};
    }

    return std::string(1, normalized[suffixStart + 1]);
}

} // namespace classmngr::engine
