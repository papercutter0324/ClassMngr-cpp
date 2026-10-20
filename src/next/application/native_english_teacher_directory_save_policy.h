#pragma once

#include "next/application/birthday_validation.h"
#include "next/application/native_english_teacher_directory_save.h"

#include <unordered_set>

namespace ClassMngr::Next::Application
{

[[nodiscard]] inline NativeEnglishTeacherDirectorySaveValidation
validateNativeEnglishTeacherDirectorySave(
    const NativeEnglishTeacherDirectorySaveRequest& request,
    NativeEnglishTeacherDirectorySaveNameKeyPolicy& nameKeyPolicy
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
                    .rowIndex =
                        NativeEnglishTeacherDirectorySaveValidationRowIndex{
                            index
                        }
                };
            }
            if (!rowIds.insert(id).second)
            {
                return {
                    .issue =
                        NativeEnglishTeacherDirectorySaveIssue::DuplicateRowId,
                    .rowIndex =
                        NativeEnglishTeacherDirectorySaveValidationRowIndex{
                            index
                        }
                };
            }
        }
        const std::u16string nameKey = nameKeyPolicy(
            std::u16string_view(row.name));
        if (nameKey.empty())
        {
            return {
                .issue = NativeEnglishTeacherDirectorySaveIssue::EmptyName,
                .rowIndex =
                    NativeEnglishTeacherDirectorySaveValidationRowIndex{
                        index
                    }
            };
        }
        if (!nameKeys.insert(nameKey).second)
        {
            return {
                .issue = NativeEnglishTeacherDirectorySaveIssue::DuplicateName,
                .rowIndex =
                    NativeEnglishTeacherDirectorySaveValidationRowIndex{
                        index
                    }
            };
        }
        if (!isBirthdayTextBlank(row.birthday)
            && !isValidMonthDayBirthday(row.birthday))
        {
            return {
                .issue = NativeEnglishTeacherDirectorySaveIssue::InvalidBirthday,
                .rowIndex =
                    NativeEnglishTeacherDirectorySaveValidationRowIndex{
                        index
                    }
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
