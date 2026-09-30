#pragma once

#include "next/application/gs_team_directory_save.h"

#include <unordered_set>

namespace ClassMngr::Next::Application
{

[[nodiscard]] inline GsTeamDirectorySaveValidation
validateGsTeamDirectorySave(
    const GsTeamDirectorySaveRequest& request
    )
{
    std::unordered_set<std::u16string> englishNameKeys;
    std::unordered_set<std::u16string> koreanNameKeys;
    for (std::size_t index = 0; index < request.rows.size(); ++index)
    {
        const GsTeamDirectorySaveRow& row = request.rows[index];
        if (row.normalizedEnglishNameKey.empty()
            && row.normalizedKoreanNameKey.empty())
        {
            return {
                .issue = GsTeamDirectorySaveIssue::MissingNames,
                .rowIndex = index
            };
        }
        if (!row.normalizedEnglishNameKey.empty()
            && !englishNameKeys.insert(row.normalizedEnglishNameKey).second)
        {
            return {
                .issue = GsTeamDirectorySaveIssue::DuplicateEnglishName,
                .rowIndex = index
            };
        }
        if (!row.normalizedKoreanNameKey.empty()
            && !koreanNameKeys.insert(row.normalizedKoreanNameKey).second)
        {
            return {
                .issue = GsTeamDirectorySaveIssue::DuplicateKoreanName,
                .rowIndex = index
            };
        }
        if (!row.birthdayIsBlank && !row.birthdayIsValid)
        {
            return {
                .issue = GsTeamDirectorySaveIssue::InvalidBirthday,
                .rowIndex = index
            };
        }
    }

    return {};
}

} // namespace ClassMngr::Next::Application
