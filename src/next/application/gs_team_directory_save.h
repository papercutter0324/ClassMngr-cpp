#pragma once

#include "next/domain/gs_team_member_id.h"
#include "next/domain/operation_result.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

// The UI supplies Qt-normalized name keys and birthday facts so the save
// policy can stay independent of Qt while owning its business decisions.
struct GsTeamDirectorySaveRow final
{
    // A missing ID explicitly represents a row that must be inserted.
    std::optional<Domain::GsTeamMemberId> id;
    std::u16string name;
    std::u16string koreanName;
    std::u16string position;
    std::u16string phoneNumber;
    std::u16string birthday;
    std::u16string normalizedEnglishNameKey;
    std::u16string normalizedKoreanNameKey;
    bool birthdayIsBlank = false;
    bool birthdayIsValid = false;

    friend bool operator==(
        const GsTeamDirectorySaveRow&,
        const GsTeamDirectorySaveRow&
        ) = default;
};

struct GsTeamDirectorySaveRequest final
{
    std::vector<GsTeamDirectorySaveRow> rows;
    std::vector<Domain::GsTeamMemberId> deletedIds;

    friend bool operator==(
        const GsTeamDirectorySaveRequest&,
        const GsTeamDirectorySaveRequest&
        ) = default;
};

enum class GsTeamDirectorySaveIssue
{
    None,
    MissingNames,
    DuplicateEnglishName,
    DuplicateKoreanName,
    InvalidBirthday
};

struct GsTeamDirectorySaveValidation final
{
    GsTeamDirectorySaveIssue issue = GsTeamDirectorySaveIssue::None;
    std::size_t rowIndex = 0;

    [[nodiscard]] bool isValid() const noexcept
    {
        return issue == GsTeamDirectorySaveIssue::None;
    }

    friend bool operator==(
        const GsTeamDirectorySaveValidation&,
        const GsTeamDirectorySaveValidation&
        ) = default;
};

struct GsTeamDirectorySaveOutcome final
{
    bool saved = false;
    GsTeamDirectorySaveValidation validation;
    std::optional<Domain::OperationError> error;

    [[nodiscard]] static GsTeamDirectorySaveOutcome success()
    {
        return {.saved = true};
    }

    [[nodiscard]] static GsTeamDirectorySaveOutcome invalid(
        GsTeamDirectorySaveValidation validation
        )
    {
        return {.validation = validation};
    }

    [[nodiscard]] static GsTeamDirectorySaveOutcome failure(
        Domain::OperationError error
        )
    {
        return {.error = std::move(error)};
    }
};

} // namespace ClassMngr::Next::Application
