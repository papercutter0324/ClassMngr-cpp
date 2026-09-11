#include "pch.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation::MainWindowDetail
{

void appendJsonEscaped(std::string& output, std::string_view value)
{
    output.push_back('"');
    for (const unsigned char character : value)
    {
        switch (character)
        {
        case '"':
            output += "\\\"";
            break;
        case '\\':
            output += "\\\\";
            break;
        case '\b':
            output += "\\b";
            break;
        case '\f':
            output += "\\f";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            if (character < 0x20)
            {
                constexpr char hex[] = "0123456789abcdef";
                output += "\\u00";
                output.push_back(hex[(character >> 4) & 0x0f]);
                output.push_back(hex[character & 0x0f]);
            }
            else
            {
                output.push_back(static_cast<char>(character));
            }
            break;
        }
    }
    output.push_back('"');
}

std::string campusResourceFileName(std::string_view reference)
{
    const std::size_t separator = reference.find_last_of("/\\");
    const std::string_view name = separator == std::string_view::npos
        ? reference
        : reference.substr(separator + 1);
    if (name.empty() || name == "." || name == ".."
        || name.find_first_of("<>:\"/\\|?*") != std::string_view::npos
        || name.back() == '.' || name.back() == ' ')
    {
        return {};
    }
    return std::string(name);
}

std::string uniqueCampusResourceFileName(
    std::string name,
    std::set<std::string>& usedNames
    )
{
    if (usedNames.insert(name).second)
    {
        return name;
    }

    const std::size_t extension = name.find_last_of('.');
    const std::string stem = extension == std::string::npos
        ? name
        : name.substr(0, extension);
    const std::string suffix = extension == std::string::npos
        ? std::string{}
        : name.substr(extension);
    for (std::size_t index = 2;; ++index)
    {
        std::string candidate = stem + "_" + std::to_string(index) + suffix;
        if (usedNames.insert(candidate).second)
        {
            return candidate;
        }
    }
}

bool isKnownPageId(std::wstring_view pageId) noexcept
{
    return pageId == homePageId
        || pageId == personalDetailsPageId
        || pageId == koreanTeachersPageId
        || pageId == nativeEnglishTeachersPageId
        || pageId == gsTeamPageId
        || pageId == subPrepPageId
        || pageId == classesPageId
        || pageId == classDetailsPageId
        || pageId == classRosterPageId
        || pageId == classSpeakingEvaluationsPageId
        || pageId == classAnalyticsPageId
        || pageId == classNotesPageId
        || pageId == aboutPageId
        || pageId == campusInformationPageId
        || pageId == campusDirectionsPageId
        || pageId == campusAddressPageId
        || pageId == campusHousingPageId
        || pageId == campusMapPageId;
}

bool isClassesPageId(std::wstring_view pageId) noexcept
{
    return pageId == classesPageId
        || pageId == classDetailsPageId
        || pageId == classRosterPageId
        || pageId == classSpeakingEvaluationsPageId
        || pageId == classAnalyticsPageId
        || pageId == classNotesPageId;
}

bool isCampusPageId(std::wstring_view pageId) noexcept
{
    return pageId == campusInformationPageId
        || pageId == campusDirectionsPageId
        || pageId == campusAddressPageId
        || pageId == campusHousingPageId
        || pageId == campusMapPageId;
}

std::string asUtf8(std::wstring_view value)
{
    return winrt::to_string(winrt::hstring(value));
}

bool isSupportedDatabasePath(std::wstring_view path) noexcept
{
    try
    {
        return classmngr::engine::DatabaseFileFormat::isSupportedInputPath(
            asUtf8(path)
            );
    }
    catch (...)
    {
        return false;
    }
}

bool pathExists(std::wstring_view path) noexcept
{
    std::error_code error;
    return std::filesystem::exists(
        std::filesystem::path(std::wstring(path)),
        error
        ) && !error;
}

std::wstring absolutePath(std::wstring_view path)
{
    std::error_code error;
    const auto absolute = std::filesystem::absolute(
        std::filesystem::path(std::wstring(path)),
        error
        );
    return error ? std::wstring(path) : absolute.wstring();
}

bool samePath(std::wstring_view lhs, std::wstring_view rhs) noexcept
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < lhs.size(); ++index)
    {
        wchar_t left = lhs[index];
        wchar_t right = rhs[index];
        if (left >= L'A' && left <= L'Z')
        {
            left = static_cast<wchar_t>(left - L'A' + L'a');
        }
        if (right >= L'A' && right <= L'Z')
        {
            right = static_cast<wchar_t>(right - L'A' + L'a');
        }
        if (left == L'\\')
        {
            left = L'/';
        }
        if (right == L'\\')
        {
            right = L'/';
        }
        if (left != right)
        {
            return false;
        }
    }
    return true;
}

std::vector<std::wstring> pruneRecentDatabasePaths(
    std::vector<std::wstring> const& paths
    )
{
    std::vector<std::wstring> result;
    result.reserve(std::min(paths.size(), maximumRecentDatabasePaths));
    for (std::wstring const& path : paths)
    {
        if (!isSupportedDatabasePath(path) || !pathExists(path))
        {
            continue;
        }
        if (std::any_of(
                result.begin(),
                result.end(),
                [&path](std::wstring const& existing) {
                    return samePath(existing, path);
                }
                ))
        {
            continue;
        }
        result.emplace_back(path);
        if (result.size() == maximumRecentDatabasePaths)
        {
            break;
        }
    }
    return result;
}

bool rosterRowHasData(
    std::vector<std::string> const& row
    )
{
    return std::any_of(
        row.cbegin(),
        row.cend(),
        [](std::string const& value) {
            return value.find_first_not_of(" \t\r\n") != std::string::npos;
        }
        );
}

bool rosterColumnEquals(
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

bool rosterRequiredColumn(std::string_view column)
{
    return std::any_of(
        classmngr::engine::RosterBaseColumns.cbegin(),
        classmngr::engine::RosterBaseColumns.cend(),
        [column](std::string_view required) {
            return rosterColumnEquals(column, required);
        }
        );
}

void padRosterRows(classmngr::engine::Roster& roster)
{
    if (roster.rows.size() > classmngr::engine::RosterValidator::MaximumRows)
    {
        roster.rows.resize(classmngr::engine::RosterValidator::MaximumRows);
    }

    for (auto& row : roster.rows)
    {
        row.resize(roster.columns.size());
    }
    roster.rows.resize(
        classmngr::engine::RosterValidator::MaximumRows,
        std::vector<std::string>(roster.columns.size())
        );
}

bool rosterHasAvailableRow(
    classmngr::engine::Roster const& roster
    )
{
    for (std::size_t row = 0;
         row < classmngr::engine::RosterValidator::MaximumRows;
         ++row)
    {
        if (row >= roster.rows.size() || !rosterRowHasData(roster.rows[row]))
        {
            return true;
        }
    }
    return false;
}

std::string rosterStudentNamePairKey(
    classmngr::engine::Roster const& roster,
    std::vector<std::string> const& row
    )
{
    const auto valueFor = [&roster, &row](std::string_view columnName) {
        const auto column = std::find(
            roster.columns.cbegin(),
            roster.columns.cend(),
            columnName
            );
        if (column == roster.columns.cend())
        {
            return std::string{};
        }
        const auto index = static_cast<std::size_t>(
            std::distance(roster.columns.cbegin(), column)
            );
        return index < row.size() ? row[index] : std::string{};
    };
    return classmngr::engine::StudentNameService::namePairKey(
        valueFor("English"),
        valueFor("Korean")
        );
}

std::wstring asWString(winrt::hstring const& value)
{
    return std::wstring(value.c_str(), value.size());
}

std::wstring asWide(std::string_view value)
{
    return asWString(winrt::to_hstring(std::string(value)));
}

std::string normalizeSpeakingAiLineEndings(std::string value)
{
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        if (value[index] != '\r')
        {
            continue;
        }
        value[index] = '\n';
        if (index + 1 < value.size() && value[index + 1] == '\n')
        {
            value.erase(index + 1, 1);
        }
    }
    return value;
}

SpeakingAiPrivateNotes splitSpeakingAiPrivateNotes(std::string notes)
{
    notes = normalizeSpeakingAiLineEndings(std::move(notes));
    constexpr std::string_view didWellMarker = "[Did Well]\n";
    constexpr std::string_view needsImprovementMarker =
        "\n[Needs Improvement]\n";
    if (!notes.starts_with(didWellMarker))
    {
        return {std::move(notes), {}};
    }

    const std::size_t separator = notes.find(
        needsImprovementMarker,
        didWellMarker.size()
        );
    if (separator == std::string::npos)
    {
        return {
            notes.substr(didWellMarker.size()),
            {}
        };
    }
    return {
        notes.substr(
            didWellMarker.size(),
            separator - didWellMarker.size()
            ),
        notes.substr(separator + needsImprovementMarker.size())
    };
}

std::string joinSpeakingAiPrivateNotes(
    std::string didWell,
    std::string needsImprovement
    )
{
    didWell = normalizeSpeakingAiLineEndings(std::move(didWell));
    needsImprovement = normalizeSpeakingAiLineEndings(std::move(needsImprovement));
    if (didWell.empty() && needsImprovement.empty())
    {
        return {};
    }

    return "[Did Well]\n" + didWell
        + "\n[Needs Improvement]\n" + needsImprovement;
}

std::string bulletizeSpeakingAiNotes(std::string notes)
{
    notes = normalizeSpeakingAiLineEndings(std::move(notes));
    if (notes.empty())
    {
        return {};
    }

    std::string result;
    std::size_t lineStart = 0;
    while (lineStart <= notes.size())
    {
        const std::size_t lineEnd = notes.find('\n', lineStart);
        std::string_view line = std::string_view(notes).substr(
            lineStart,
            (lineEnd == std::string::npos ? notes.size() : lineEnd) - lineStart
            );
        if (!result.empty())
        {
            result.push_back('\n');
        }
        if (!line.empty() && !line.starts_with("\xE2\x80\xA2"))
        {
            result += "\xE2\x80\xA2 ";
        }
        result += line;
        if (lineEnd == std::string::npos)
        {
            break;
        }
        lineStart = lineEnd + 1;
    }
    return result;
}

void replaceSpeakingAiPlaceholder(
    std::wstring& value,
    std::wstring_view replacement
    )
{
    constexpr std::wstring_view placeholder = L"STD_NAME";
    std::size_t position = 0;
    while ((position = value.find(placeholder, position))
           != std::wstring::npos)
    {
        value.replace(position, placeholder.size(), replacement);
        position += replacement.size();
    }
}

std::string speakingAiStudentId(std::size_t row)
{
    std::string result = "STUDENT_";
    const std::size_t number = row + 1;
    if (number < 10)
    {
        result.push_back('0');
    }
    result += std::to_string(number);
    return result;
}

} // namespace winrt::ClassMngrWinUI::implementation::MainWindowDetail
