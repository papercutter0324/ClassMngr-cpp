#include "classmngr/engine/database_file_format.h"

#include <cctype>

namespace classmngr::engine::DatabaseFileFormat
{
namespace
{
bool isAsciiWhitespace(
    char character
    ) noexcept
{
    return std::isspace(static_cast<unsigned char>(character)) != 0;
}

bool isBlank(
    std::string_view text
    ) noexcept
{
    if (text.empty())
    {
        return true;
    }

    for (const char character : text)
    {
        if (!isAsciiWhitespace(character))
        {
            return false;
        }
    }

    return true;
}

bool equalsAsciiCaseInsensitive(
    char lhs,
    char rhs
    ) noexcept
{
    if (lhs >= 'A' && lhs <= 'Z')
    {
        lhs = static_cast<char>(lhs - 'A' + 'a');
    }
    if (rhs >= 'A' && rhs <= 'Z')
    {
        rhs = static_cast<char>(rhs - 'A' + 'a');
    }

    return lhs == rhs;
}

bool endsWithAsciiCaseInsensitive(
    std::string_view text,
    std::string_view suffix
    ) noexcept
{
    if (text.size() < suffix.size())
    {
        return false;
    }

    const std::size_t offset = text.size() - suffix.size();
    for (std::size_t index = 0; index < suffix.size(); ++index)
    {
        if (!equalsAsciiCaseInsensitive(text[offset + index], suffix[index]))
        {
            return false;
        }
    }

    return true;
}

std::string_view fileName(
    std::string_view filePath
    ) noexcept
{
    const std::size_t separator = filePath.find_last_of("/\\");
    if (separator == std::string_view::npos)
    {
        return filePath;
    }

    return filePath.substr(separator + 1);
}

bool hasFileSuffix(
    std::string_view filePath
    ) noexcept
{
    const std::string_view name = fileName(filePath);
    const std::size_t separator = name.find_last_of('.');

    return separator != std::string_view::npos
        && separator + 1 < name.size();
}
}

bool isNativePath(
    std::string_view filePath
    ) noexcept
{
    return endsWithAsciiCaseInsensitive(filePath, nativeExtension());
}

bool isLegacyPath(
    std::string_view filePath
    ) noexcept
{
    return endsWithAsciiCaseInsensitive(filePath, legacyExtension());
}

bool isSupportedInputPath(
    std::string_view filePath
    ) noexcept
{
    return isNativePath(filePath) || isLegacyPath(filePath);
}

std::string nativeOutputPath(
    std::string_view filePath
    )
{
    if (isBlank(filePath) || isNativePath(filePath))
    {
        return std::string(filePath);
    }

    std::string result(filePath);
    if (isLegacyPath(filePath))
    {
        result.resize(result.size() - legacyExtension().size());
    }

    result += nativeExtension();
    return result;
}

std::string supportedInputPath(
    std::string_view filePath
    )
{
    if (isBlank(filePath) || isSupportedInputPath(filePath))
    {
        return std::string(filePath);
    }

    if (!hasFileSuffix(filePath))
    {
        return std::string(filePath) + std::string(nativeExtension());
    }

    return std::string(filePath);
}

} // namespace classmngr::engine::DatabaseFileFormat
