#pragma once

#include "next/application/birthday_validation.h"
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
    std::unordered_set<int> rowIds;
    for (std::size_t index = 0; index < request.rows.size(); ++index)
    {
        const NativeEnglishTeacherDirectorySaveRow& row = request.rows[index];
        if (row.id.has_value())
        {
            const int id = row.id->value();
            if (id <= 0)
            {
                return {
                    .issue =
                        NativeEnglishTeacherDirectorySaveIssue::InvalidRowId,
                    .rowIndex = index
                };
            }
            if (!rowIds.insert(id).second)
            {
                return {
                    .issue =
                        NativeEnglishTeacherDirectorySaveIssue::DuplicateRowId,
                    .rowIndex = index
                };
            }
        }
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
        if (!isBirthdayTextBlank(row.birthday)
            && !isValidMonthDayBirthday(row.birthday))
        {
            return {
                .issue = NativeEnglishTeacherDirectorySaveIssue::InvalidBirthday,
                .rowIndex = index
            };
        }
    }

    for (const Domain::NativeEnglishTeacherId deletedId : request.deletedIds)
    {
        if (deletedId.value() <= 0)
        {
            return {
                .issue =
                    NativeEnglishTeacherDirectorySaveIssue::InvalidDeletedId
            };
        }
    }

    return {};
}

} // namespace ClassMngr::Next::Application
