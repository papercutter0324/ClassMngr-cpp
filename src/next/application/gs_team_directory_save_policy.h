#pragma once

#include "next/application/birthday_validation.h"
#include "next/application/gs_team_directory_save.h"

#include <unordered_set>

namespace ClassMngr::Next::Application
{

[[nodiscard]] inline GsTeamDirectorySaveValidation
validateGsTeamDirectorySave(
    const GsTeamDirectorySaveRequest& request,
    GsTeamDirectorySaveNameKeyPolicy& nameKeyPolicy
    )
{
    std::unordered_set<std::u16string> englishNameKeys;
    std::unordered_set<std::u16string> koreanNameKeys;
    std::unordered_set<int> rowIds;
    for (std::size_t index = 0; index < request.rows.size(); ++index)
    {
        const GsTeamDirectorySaveRow& row = request.rows[index];
        if (row.id.has_value())
        {
            const int id = row.id->value();
            if (id <= 0)
            {
                return {
                    .issue = GsTeamDirectorySaveIssue::InvalidRowId,
                    .rowIndex = index
                };
            }
            if (!rowIds.insert(id).second)
            {
                return {
                    .issue = GsTeamDirectorySaveIssue::DuplicateRowId,
                    .rowIndex = index
                };
            }
        }
        const std::u16string englishNameKey = nameKeyPolicy(row.name);
        const std::u16string koreanNameKey = nameKeyPolicy(row.koreanName);
        if (englishNameKey.empty() && koreanNameKey.empty())
        {
            return {
                .issue = GsTeamDirectorySaveIssue::MissingNames,
                .rowIndex = index
            };
        }
        if (!englishNameKey.empty()
            && !englishNameKeys.insert(englishNameKey).second)
        {
            return {
                .issue = GsTeamDirectorySaveIssue::DuplicateEnglishName,
                .rowIndex = index
            };
        }
        if (!koreanNameKey.empty()
            && !koreanNameKeys.insert(koreanNameKey).second)
        {
            return {
                .issue = GsTeamDirectorySaveIssue::DuplicateKoreanName,
                .rowIndex = index
            };
        }
        if (!isBirthdayTextBlank(row.birthday)
            && !isValidMonthDayBirthday(row.birthday))
        {
            return {
                .issue = GsTeamDirectorySaveIssue::InvalidBirthday,
                .rowIndex = index
            };
        }
    }

    for (const Domain::GsTeamMemberId deletedId : request.deletedIds)
    {
        if (deletedId.value() <= 0)
        {
            return {
                .issue = GsTeamDirectorySaveIssue::InvalidDeletedId
            };
        }
    }

    return {};
}

} // namespace ClassMngr::Next::Application
