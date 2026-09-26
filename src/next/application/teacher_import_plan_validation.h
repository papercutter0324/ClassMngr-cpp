#pragma once

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace ClassMngr::Next::Application
{

enum class TeacherImportPlanValidationIssue
{
    None,
    ReviewSelectionMismatch,
    InvalidSourceDate,
    MissingKoreanTeacherName,
    DuplicateKoreanTeacherName,
    MissingNativeEnglishTeacherName,
    DuplicateNativeEnglishTeacherName,
    MissingGsTeamMemberName,
    DuplicateGsTeamMemberName
};

struct TeacherImportGsTeamNameKeys
{
    std::string english;
    std::string korean;
};

// The adapter supplies normalized identity keys and preserves source order.
// English and Korean GS Team keys are intentionally checked in separate
// namespaces, matching the import's existing identity rules.
struct TeacherImportPlanValidationInput
{
    bool sourceDateValid = false;
    std::optional<std::vector<std::string>> reviewedKoreanTeacherKeys;
    std::vector<std::string> koreanTeacherKeys;
    std::vector<std::string> nativeEnglishTeacherKeys;
    std::vector<TeacherImportGsTeamNameKeys> gsTeamMemberKeys;
};

[[nodiscard]] inline TeacherImportPlanValidationIssue validateTeacherImportPlan(
    const TeacherImportPlanValidationInput& input
    )
{
    if (input.reviewedKoreanTeacherKeys
        && *input.reviewedKoreanTeacherKeys != input.koreanTeacherKeys)
    {
        return TeacherImportPlanValidationIssue::ReviewSelectionMismatch;
    }

    if (!input.sourceDateValid)
    {
        return TeacherImportPlanValidationIssue::InvalidSourceDate;
    }

    std::unordered_set<std::string> koreanTeacherKeys;
    for (const std::string& key : input.koreanTeacherKeys)
    {
        if (key.empty())
        {
            return TeacherImportPlanValidationIssue::MissingKoreanTeacherName;
        }
        if (!koreanTeacherKeys.insert(key).second)
        {
            return TeacherImportPlanValidationIssue::DuplicateKoreanTeacherName;
        }
    }

    std::unordered_set<std::string> nativeEnglishTeacherKeys;
    for (const std::string& key : input.nativeEnglishTeacherKeys)
    {
        if (key.empty())
        {
            return TeacherImportPlanValidationIssue::MissingNativeEnglishTeacherName;
        }
        if (!nativeEnglishTeacherKeys.insert(key).second)
        {
            return TeacherImportPlanValidationIssue::DuplicateNativeEnglishTeacherName;
        }
    }

    std::unordered_set<std::string> gsEnglishKeys;
    std::unordered_set<std::string> gsKoreanKeys;
    for (const TeacherImportGsTeamNameKeys& member : input.gsTeamMemberKeys)
    {
        if (member.english.empty() && member.korean.empty())
        {
            return TeacherImportPlanValidationIssue::MissingGsTeamMemberName;
        }
        if ((!member.english.empty() && gsEnglishKeys.contains(member.english))
            || (!member.korean.empty() && gsKoreanKeys.contains(member.korean)))
        {
            return TeacherImportPlanValidationIssue::DuplicateGsTeamMemberName;
        }
        if (!member.english.empty())
        {
            gsEnglishKeys.insert(member.english);
        }
        if (!member.korean.empty())
        {
            gsKoreanKeys.insert(member.korean);
        }
    }

    return TeacherImportPlanValidationIssue::None;
}

} // namespace ClassMngr::Next::Application
