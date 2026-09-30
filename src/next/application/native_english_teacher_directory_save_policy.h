#pragma once

#include "next/application/native_english_teacher_directory_save.h"

#include <unordered_set>

namespace ClassMngr::Next::Application
{

[[nodiscard]] inline NativeEnglishTeacherDirectorySaveValidation
validateNativeEnglishTeacherDirectorySave(
    const NativeEnglishTeacherDirectorySaveRequest& request
    )
{
    std::unordered_set<std::u16string> nameKeys;
    for (std::size_t index = 0; index < request.rows.size(); ++index)
    {
        const NativeEnglishTeacherDirectorySaveRow& row = request.rows[index];
        if (row.normalizedNameKey.empty())
        {
            return {
                .issue = NativeEnglishTeacherDirectorySaveIssue::EmptyName,
                .rowIndex = index
            };
        }
        if (!nameKeys.insert(row.normalizedNameKey).second)
        {
            return {
                .issue = NativeEnglishTeacherDirectorySaveIssue::DuplicateName,
                .rowIndex = index
            };
        }
        if (!row.birthdayIsBlank && !row.birthdayIsValid)
        {
            return {
                .issue = NativeEnglishTeacherDirectorySaveIssue::InvalidBirthday,
                .rowIndex = index
            };
        }
    }

    return {};
}

} // namespace ClassMngr::Next::Application
