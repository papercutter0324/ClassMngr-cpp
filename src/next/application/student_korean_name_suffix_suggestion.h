#pragma once

#include "next/application/qt_compatible_text.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct StudentKoreanNameSuggestionRow final
{
    std::u16string englishName;
    std::u16string koreanBaseName;
    std::optional<char16_t> koreanNameSuffix;
};

// Suggests the first unused uppercase suffix for the selected student's exact
// trimmed English name and legacy-projected Korean base. The caller supplies
// the base and suffix from StudentNameUtils so Qt regex edge cases remain at
// the model boundary. Invalid columns, rows, or incomplete selected names
// produce an empty result.
[[nodiscard]] inline std::u16string suggestStudentKoreanNameSuffix(
    const std::vector<StudentKoreanNameSuggestionRow>& rows,
    const int selectedRow,
    const bool hasEnglishColumn,
    const bool hasKoreanColumn
    )
{
    if (!hasEnglishColumn
        || !hasKoreanColumn
        || selectedRow < 0
        || static_cast<std::size_t>(selectedRow) >= rows.size())
    {
        return {};
    }

    const StudentKoreanNameSuggestionRow& selected =
        rows[static_cast<std::size_t>(selectedRow)];
    const std::u16string englishName = trimQtWhitespace(selected.englishName);
    if (englishName.empty() || selected.koreanBaseName.empty())
    {
        return {};
    }

    std::array<bool, 26> usedSuffixes{};
    for (const StudentKoreanNameSuggestionRow& candidate : rows)
    {
        if (englishName != trimQtWhitespace(candidate.englishName)
            || selected.koreanBaseName != candidate.koreanBaseName)
        {
            continue;
        }

        const auto suffix = candidate.koreanNameSuffix;
        if (suffix && *suffix >= u'A' && *suffix <= u'Z')
        {
            usedSuffixes[static_cast<std::size_t>(*suffix - u'A')] = true;
        }
    }

    for (std::size_t index = 0; index < usedSuffixes.size(); ++index)
    {
        if (usedSuffixes[index])
        {
            continue;
        }

        std::u16string suggestion = selected.koreanBaseName;
        suggestion.push_back(u'(');
        suggestion.push_back(static_cast<char16_t>(u'A' + index));
        suggestion.push_back(u')');
        return suggestion;
    }

    return {};
}

} // namespace ClassMngr::Next::Application
