#include "classmngr/engine/document_catalog.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <set>
#include <utility>

namespace classmngr::engine
{
namespace
{
Error invalidFormat(std::string message)
{
    return {
        ErrorCode::InvalidFormat,
        std::move(message),
        std::nullopt
    };
}

std::string trimAsciiWhitespace(std::string_view value)
{
    std::size_t first = 0;
    while (
        first < value.size()
        && std::isspace(static_cast<unsigned char>(value[first])) != 0
        )
    {
        ++first;
    }

    std::size_t last = value.size();
    while (
        last > first
        && std::isspace(static_cast<unsigned char>(value[last - 1])) != 0
        )
    {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

bool isAbsoluteDirectoryPath(std::string_view path)
{
    if (path.empty() || path.front() == '/')
    {
        return true;
    }

    // A drive-qualified path is absolute or drive-relative on Windows.  Both
    // forms must not be allowed to escape the catalog's resource root.
    return path.size() >= 2
        && path[1] == ':'
        && (
            (path[0] >= 'A' && path[0] <= 'Z')
            || (path[0] >= 'a' && path[0] <= 'z')
            );
}

bool asciiAlphaNumeric(char value)
{
    return (
        value >= 'A' && value <= 'Z'
        ) || (
        value >= 'a' && value <= 'z'
        ) || (
        value >= '0' && value <= '9'
        );
}

bool unicodeLetterOrNumber(char32_t codePoint)
{
    // The catalog's identifiers are normally ASCII, but Qt's former
    // QChar::isLetterOrNumber validation also accepted the common Unicode
    // scripts used by localized resource packs.  Keep that behavior without
    // making the engine depend on a platform text library.
    constexpr std::array<std::pair<char32_t, char32_t>, 23> ranges{
        std::pair{0x00C0, 0x02AF}, // Latin, IPA, and modifier letters
        std::pair{0x0370, 0x052F}, // Greek and Cyrillic
        std::pair{0x0531, 0x058F}, // Armenian
        std::pair{0x0591, 0x05FF}, // Hebrew
        std::pair{0x0600, 0x06FF}, // Arabic
        std::pair{0x0710, 0x074F}, // Syriac and Thaana
        std::pair{0x0780, 0x07BF}, // NKo and Samaritan
        std::pair{0x0900, 0x0DFF}, // Indic scripts
        std::pair{0x0E00, 0x0FFF}, // Southeast Asian scripts
        std::pair{0x1000, 0x1FFF}, // Southeast Asian and African scripts
        std::pair{0x2D30, 0x2D7F}, // Tifinagh
        std::pair{0x3041, 0x30FA}, // Hiragana and Katakana letters
        std::pair{0x3105, 0x312F}, // Bopomofo
        std::pair{0x3131, 0x318E}, // Hangul compatibility jamo
        std::pair{0x31A0, 0x31BF}, // Bopomofo extensions
        std::pair{0x3400, 0x4DBF}, // CJK extension A
        std::pair{0x4E00, 0xA4CF}, // CJK and Yi
        std::pair{0xA500, 0xA7FF}, // Vai and Latin extensions
        std::pair{0xAC00, 0xD7A3}, // Hangul syllables
        std::pair{0xF900, 0xFAFF}, // CJK compatibility ideographs
        std::pair{0xFB00, 0xFDFF}, // Alphabetic presentation forms
        std::pair{0xFE70, 0xFEFF}, // Arabic presentation forms
        std::pair{0xFF10, 0xFF5A}  // Fullwidth digits and letters
    };

    for (const auto& [first, last] : ranges)
    {
        if (codePoint >= first && codePoint <= last)
        {
            return true;
        }
    }

    // Supplementary-plane CJK and historic scripts are also letters or
    // numbers when they are present in a UTF-8 identifier.
    return codePoint >= 0x10000 && codePoint <= 0x1FAFF;
}

bool decodeUtf8CodePoint(
    std::string_view value,
    std::size_t& index,
    char32_t& codePoint
    )
{
    const auto byteAt = [&value](std::size_t position)
    {
        return static_cast<unsigned char>(value[position]);
    };
    const auto continuation = [&byteAt](std::size_t position)
    {
        return (byteAt(position) & 0xC0) == 0x80;
    };

    const unsigned char first = byteAt(index);
    if (first < 0x80)
    {
        codePoint = first;
        ++index;
        return true;
    }

    std::size_t width = 0;
    char32_t decoded = 0;
    char32_t minimum = 0;
    if ((first & 0xE0) == 0xC0)
    {
        width = 2;
        decoded = first & 0x1F;
        minimum = 0x80;
    }
    else if ((first & 0xF0) == 0xE0)
    {
        width = 3;
        decoded = first & 0x0F;
        minimum = 0x800;
    }
    else if ((first & 0xF8) == 0xF0)
    {
        width = 4;
        decoded = first & 0x07;
        minimum = 0x10000;
    }
    else
    {
        return false;
    }

    if (index + width > value.size())
    {
        return false;
    }
    for (std::size_t offset = 1; offset < width; ++offset)
    {
        if (!continuation(index + offset))
        {
            return false;
        }
        decoded = (decoded << 6) | (byteAt(index + offset) & 0x3F);
    }

    if (
        decoded < minimum
        || decoded > 0x10FFFF
        || (decoded >= 0xD800 && decoded <= 0xDFFF)
        )
    {
        return false;
    }

    codePoint = decoded;
    index += width;
    return true;
}

std::string normalizedLocale(std::string_view localeName)
{
    std::string result = trimAsciiWhitespace(localeName);
    std::replace(result.begin(), result.end(), '-', '_');
    return result;
}

std::string duplicateFolderIdWarning(std::string_view id)
{
    return "folder '" + std::string(id)
        + "' duplicates folder id '" + std::string(id)
        + "'. The folder was skipped.";
}

std::string duplicateFolderPathWarning(
    std::string_view id,
    std::string_view path
    )
{
    return "folder '" + std::string(id)
        + "' duplicates folder path '" + std::string(path)
        + "'. The folder was skipped.";
}

std::string unreachableFolderWarning(std::string_view id)
{
    return "folder '" + std::string(id)
        + "' has no valid metadata for an ancestor and will not be displayed.";
}

std::string duplicateDocumentWarning(std::string_view id)
{
    return "document '" + std::string(id)
        + "' duplicates document id '" + std::string(id)
        + "'. The document was skipped.";
}

std::string unreachableDocumentWarning(
    std::string_view id,
    std::string_view path
    )
{
    return "document '" + std::string(id)
        + "' references folder '" + std::string(path)
        + "' without valid metadata. The document was skipped.";
}
} // namespace

std::string DocumentLocalizedNames::forLocale(
    std::string_view localeName
    ) const
{
    const std::string normalized = normalizedLocale(localeName);
    const auto exact = localeNames.find(normalized);
    if (exact != localeNames.end())
    {
        return exact->second;
    }

    const std::size_t separator = normalized.find('_');
    const std::string language = normalized.substr(
        0,
        separator == std::string::npos ? normalized.size() : separator
        );
    const auto languageMatch = localeNames.find(language);
    return languageMatch == localeNames.end()
        ? defaultName
        : languageMatch->second;
}

Result<std::string> normalizeRelativeDirectoryPath(
    std::string_view path
    )
{
    std::string normalized = trimAsciiWhitespace(path);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    if (normalized.empty() || isAbsoluteDirectoryPath(normalized))
    {
        return std::unexpected(invalidFormat(
            "must be a non-empty relative directory path."
            ));
    }

    std::size_t segmentStart = 0;
    while (segmentStart <= normalized.size())
    {
        const std::size_t separator = normalized.find('/', segmentStart);
        const std::size_t segmentEnd = separator == std::string::npos
            ? normalized.size()
            : separator;
        const std::string_view segment(
            normalized.data() + segmentStart,
            segmentEnd - segmentStart
            );

        if (segment.empty() || segment == "." || segment == "..")
        {
            return std::unexpected(invalidFormat(
                "contains an unsafe path segment."
                ));
        }

        if (separator == std::string::npos)
        {
            break;
        }
        segmentStart = separator + 1;
    }

    return normalized;
}

Result<std::string> validatePlainFileName(
    std::string_view fileName
    )
{
    const std::string normalized = trimAsciiWhitespace(fileName);
    if (
        normalized.empty()
        || normalized == "."
        || normalized == ".."
        || normalized.find('/') != std::string::npos
        || normalized.find('\\') != std::string::npos
        || (
            normalized.size() >= 2
            && normalized[1] == ':'
            && (
                (normalized[0] >= 'A' && normalized[0] <= 'Z')
                || (normalized[0] >= 'a' && normalized[0] <= 'z')
                )
            )
        )
    {
        return std::unexpected(invalidFormat(
            "must be a plain file name."
            ));
    }

    return normalized;
}

Result<int> validateOrder(long long order)
{
    if (order < 0 || order > std::numeric_limits<int>::max())
    {
        return std::unexpected(invalidFormat(
            "must be a non-negative integer."
            ));
    }

    return static_cast<int>(order);
}

bool validIdentifier(std::string_view id)
{
    if (id.empty())
    {
        return false;
    }

    for (std::size_t index = 0; index < id.size();)
    {
        if (static_cast<unsigned char>(id[index]) < 0x80)
        {
            const char character = id[index++];
            if (
                !asciiAlphaNumeric(character)
                && character != '_'
                && character != '-'
                )
            {
                return false;
            }
            continue;
        }

        char32_t codePoint = 0;
        if (
            !decodeUtf8CodePoint(id, index, codePoint)
            || !unicodeLetterOrNumber(codePoint)
            )
        {
            return false;
        }
    }

    return true;
}

std::string parentPath(std::string_view path)
{
    const std::size_t separator = path.rfind('/');
    return separator == std::string::npos
        ? std::string()
        : std::string(path.substr(0, separator));
}

Result<DocumentLocalizedNames> normalizeLocalizedNames(
    std::string_view defaultName,
    const std::map<std::string, std::string>& localeNames
    )
{
    DocumentLocalizedNames result;
    result.defaultName = trimAsciiWhitespace(defaultName);
    if (result.defaultName.empty())
    {
        return std::unexpected(invalidFormat(
            "default is required."
            ));
    }

    for (const auto& [locale, name] : localeNames)
    {
        const std::string normalizedLocaleName = normalizedLocale(locale);
        const std::string normalizedName = trimAsciiWhitespace(name);
        if (normalizedLocaleName.empty())
        {
            return std::unexpected(invalidFormat(
                "contains an empty locale key."
                ));
        }
        if (normalizedName.empty())
        {
            return std::unexpected(invalidFormat(
                "locale names must be non-empty strings."
                ));
        }

        result.localeNames[normalizedLocaleName] = normalizedName;
    }

    return result;
}

DocumentCatalogModel DocumentCatalogService::build(
    const DocumentCatalogInput& input
    )
{
    DocumentCatalogModel result;
    std::set<std::string> folderIds;
    std::set<std::string> folderPaths;

    for (const DocumentFolderDefinition& folder : input.folders)
    {
        if (!folderIds.insert(folder.id).second)
        {
            result.warnings.push_back(duplicateFolderIdWarning(folder.id));
            continue;
        }
        if (!folderPaths.insert(folder.path).second)
        {
            folderIds.erase(folder.id);
            result.warnings.push_back(
                duplicateFolderPathWarning(folder.id, folder.path)
                );
            continue;
        }

        result.folders.push_back(folder);
    }

    std::set<std::string> reachableFolderPaths;
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (const DocumentFolderDefinition& folder : result.folders)
        {
            if (
                reachableFolderPaths.contains(folder.path)
                || (
                    !folder.parentPath.empty()
                    && !reachableFolderPaths.contains(folder.parentPath)
                    )
                )
            {
                continue;
            }

            reachableFolderPaths.insert(folder.path);
            changed = true;
        }
    }

    for (const DocumentFolderDefinition& folder : result.folders)
    {
        if (!reachableFolderPaths.contains(folder.path))
        {
            result.warnings.push_back(unreachableFolderWarning(folder.id));
        }
    }

    std::set<std::string> documentIds;
    for (const DocumentDefinition& document : input.documents)
    {
        if (!documentIds.insert(document.id).second)
        {
            result.warnings.push_back(duplicateDocumentWarning(document.id));
            continue;
        }
        if (!reachableFolderPaths.contains(document.folderPath))
        {
            documentIds.erase(document.id);
            result.warnings.push_back(
                unreachableDocumentWarning(document.id, document.folderPath)
                );
            continue;
        }

        result.documents.push_back(document);
    }

    result.folders.erase(
        std::remove_if(
            result.folders.begin(),
            result.folders.end(),
            [&reachableFolderPaths](const DocumentFolderDefinition& folder)
            {
                return !reachableFolderPaths.contains(folder.path);
            }
            ),
        result.folders.end()
        );

    return result;
}

} // namespace classmngr::engine
