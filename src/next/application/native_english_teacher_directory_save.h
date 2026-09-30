#pragma once

#include "next/domain/native_english_teacher_id.h"
#include "next/domain/operation_result.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

// The feature edge supplies Qt-normalized comparison and birthday facts so
// this request remains Qt-free while Application owns the validation rules.
struct NativeEnglishTeacherDirectorySaveRow final
{
    // A missing ID explicitly represents a row that must be inserted.
    std::optional<Domain::NativeEnglishTeacherId> id;
    std::u16string name;
    std::u16string position;
    std::u16string phoneNumber;
    std::u16string email;
    std::u16string birthday;
    std::u16string nationality;
    std::u16string normalizedNameKey;
    bool birthdayIsBlank = false;
    bool birthdayIsValid = false;

    friend bool operator==(
        const NativeEnglishTeacherDirectorySaveRow&,
        const NativeEnglishTeacherDirectorySaveRow&
        ) = default;
};

struct NativeEnglishTeacherDirectorySaveRequest final
{
    std::vector<NativeEnglishTeacherDirectorySaveRow> rows;
    std::vector<Domain::NativeEnglishTeacherId> deletedIds;

    friend bool operator==(
        const NativeEnglishTeacherDirectorySaveRequest&,
        const NativeEnglishTeacherDirectorySaveRequest&
        ) = default;
};

enum class NativeEnglishTeacherDirectorySaveIssue
{
    None,
    EmptyName,
    DuplicateName,
    InvalidBirthday
};

struct NativeEnglishTeacherDirectorySaveValidation final
{
    NativeEnglishTeacherDirectorySaveIssue issue =
        NativeEnglishTeacherDirectorySaveIssue::None;
    std::size_t rowIndex = 0;

    [[nodiscard]] bool isValid() const noexcept
    {
        return issue == NativeEnglishTeacherDirectorySaveIssue::None;
    }

    friend bool operator==(
        const NativeEnglishTeacherDirectorySaveValidation&,
        const NativeEnglishTeacherDirectorySaveValidation&
        ) = default;
};

struct NativeEnglishTeacherDirectorySaveOutcome final
{
    bool saved = false;
    NativeEnglishTeacherDirectorySaveValidation validation;
    std::optional<Domain::OperationError> error;

    [[nodiscard]] static NativeEnglishTeacherDirectorySaveOutcome success()
    {
        return {.saved = true};
    }

    [[nodiscard]] static NativeEnglishTeacherDirectorySaveOutcome invalid(
        NativeEnglishTeacherDirectorySaveValidation validation
        )
    {
        return {.validation = validation};
    }

    [[nodiscard]] static NativeEnglishTeacherDirectorySaveOutcome failure(
        Domain::OperationError error
        )
    {
        return {.error = std::move(error)};
    }
};

} // namespace ClassMngr::Next::Application
