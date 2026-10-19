#include "next/application/teacher_import_plan_validation.h"

#include <cstdlib>
#include <initializer_list>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using ClassMngr::Next::Application::TeacherImportPlanValidationInput;
using ClassMngr::Next::Application::TeacherImportPlanValidationIssue;
using ClassMngr::Next::Application::ImportSourceKey;
using ClassMngr::Next::Application::TeacherImportGsTeamNameKeys;
using ClassMngr::Next::Application::validateTeacherImportPlan;

static_assert(std::is_same_v<
    decltype(std::declval<TeacherImportGsTeamNameKeys>().english),
    ImportSourceKey>);
static_assert(std::is_same_v<
    decltype(std::declval<TeacherImportGsTeamNameKeys>().korean),
    ImportSourceKey>);
static_assert(std::is_same_v<
    decltype(std::declval<TeacherImportPlanValidationInput>().reviewedKoreanTeacherKeys),
    std::optional<std::vector<ImportSourceKey>>>);
static_assert(std::is_same_v<
    decltype(std::declval<TeacherImportPlanValidationInput>().koreanTeacherKeys),
    std::vector<ImportSourceKey>>);
static_assert(std::is_same_v<
    decltype(std::declval<TeacherImportPlanValidationInput>().nativeEnglishTeacherKeys),
    std::vector<ImportSourceKey>>);
static_assert(std::is_same_v<
    decltype(std::declval<TeacherImportPlanValidationInput>().gsTeamMemberKeys),
    std::vector<TeacherImportGsTeamNameKeys>>);

namespace
{

std::vector<ImportSourceKey> sourceKeys(
    std::initializer_list<std::string> values
    )
{
    std::vector<ImportSourceKey> keys;
    keys.reserve(values.size());
    for (const std::string& value : values)
    {
        keys.emplace_back(value);
    }
    return keys;
}

TeacherImportGsTeamNameKeys gsKeys(
    std::string english,
    std::string korean
    )
{
    return {
        ImportSourceKey(std::move(english)),
        ImportSourceKey(std::move(korean))
    };
}

bool hasIssue(
    const TeacherImportPlanValidationInput& input,
    const TeacherImportPlanValidationIssue expected
    )
{
    return validateTeacherImportPlan(input) == expected;
}

} // namespace

int main()
{
    TeacherImportPlanValidationInput valid;
    valid.sourceDateValid = true;
    valid.koreanTeacherKeys = sourceKeys({"korean-1", "korean-2"});
    valid.nativeEnglishTeacherKeys = sourceKeys({"native-1", "native-2"});
    valid.gsTeamMemberKeys = {
        gsKeys("english-1", "korean-1"),
        gsKeys("english-2", "korean-2")
    };
    if (!hasIssue(valid, TeacherImportPlanValidationIssue::None))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput invalidDate = valid;
    invalidDate.sourceDateValid = false;
    invalidDate.koreanTeacherKeys = sourceKeys({""});
    if (!hasIssue(invalidDate, TeacherImportPlanValidationIssue::InvalidSourceDate))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput matchingReview = valid;
    matchingReview.reviewedKoreanTeacherKeys = matchingReview.koreanTeacherKeys;
    if (!hasIssue(matchingReview, TeacherImportPlanValidationIssue::None))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput reviewCountMismatch = valid;
    reviewCountMismatch.sourceDateValid = false;
    reviewCountMismatch.reviewedKoreanTeacherKeys = sourceKeys({"korean-1"});
    if (!hasIssue(
            reviewCountMismatch,
            TeacherImportPlanValidationIssue::ReviewSelectionMismatch))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput reviewOrderMismatch = valid;
    reviewOrderMismatch.reviewedKoreanTeacherKeys = sourceKeys({"korean-2", "korean-1"});
    if (!hasIssue(
            reviewOrderMismatch,
            TeacherImportPlanValidationIssue::ReviewSelectionMismatch))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput koreanMissing = valid;
    koreanMissing.koreanTeacherKeys = sourceKeys({""});
    koreanMissing.nativeEnglishTeacherKeys = sourceKeys({""});
    if (!hasIssue(
            koreanMissing,
            TeacherImportPlanValidationIssue::MissingKoreanTeacherName))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput koreanDuplicate = valid;
    koreanDuplicate.koreanTeacherKeys = sourceKeys({"same", "same"});
    koreanDuplicate.nativeEnglishTeacherKeys = sourceKeys({""});
    if (!hasIssue(
            koreanDuplicate,
            TeacherImportPlanValidationIssue::DuplicateKoreanTeacherName))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput nativeMissing = valid;
    nativeMissing.nativeEnglishTeacherKeys = sourceKeys({""});
    nativeMissing.gsTeamMemberKeys = {gsKeys("", "")};
    if (!hasIssue(
            nativeMissing,
            TeacherImportPlanValidationIssue::MissingNativeEnglishTeacherName))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput nativeDuplicate = valid;
    nativeDuplicate.nativeEnglishTeacherKeys = sourceKeys({"same", "same"});
    if (!hasIssue(
            nativeDuplicate,
            TeacherImportPlanValidationIssue::DuplicateNativeEnglishTeacherName))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput gsMissing = valid;
    gsMissing.gsTeamMemberKeys = {gsKeys("", "")};
    if (!hasIssue(
            gsMissing,
            TeacherImportPlanValidationIssue::MissingGsTeamMemberName))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput gsEnglishDuplicate = valid;
    gsEnglishDuplicate.gsTeamMemberKeys = {
        gsKeys("same", "korean-1"),
        gsKeys("same", "korean-2")
    };
    if (!hasIssue(
            gsEnglishDuplicate,
            TeacherImportPlanValidationIssue::DuplicateGsTeamMemberName))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput gsKoreanDuplicate = valid;
    gsKoreanDuplicate.gsTeamMemberKeys = {
        gsKeys("english-1", "same"),
        gsKeys("english-2", "same")
    };
    if (!hasIssue(
            gsKoreanDuplicate,
            TeacherImportPlanValidationIssue::DuplicateGsTeamMemberName))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput gsNamespaces = valid;
    gsNamespaces.gsTeamMemberKeys = {
        gsKeys("shared", "other-1"),
        gsKeys("other-2", "shared")
    };
    if (!hasIssue(gsNamespaces, TeacherImportPlanValidationIssue::None))
    {
        return EXIT_FAILURE;
    }

    const std::string arbitraryBytes("\xFF\0\xC3\xA9", 4);
    TeacherImportPlanValidationInput byteExact = valid;
    byteExact.koreanTeacherKeys = sourceKeys({arbitraryBytes});
    byteExact.reviewedKoreanTeacherKeys = sourceKeys({arbitraryBytes});
    byteExact.nativeEnglishTeacherKeys = sourceKeys({std::string("\xFE", 1)});
    byteExact.gsTeamMemberKeys = {
        gsKeys(arbitraryBytes, std::string("\0\xFD", 2))
    };
    if (!hasIssue(byteExact, TeacherImportPlanValidationIssue::None)
        || byteExact.koreanTeacherKeys.front().value() != arbitraryBytes
        || byteExact.reviewedKoreanTeacherKeys->front().value() != arbitraryBytes
        || byteExact.gsTeamMemberKeys.front().english.value() != arbitraryBytes
        || byteExact.gsTeamMemberKeys.front().korean.value()
            != std::string("\0\xFD", 2))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput exactDuplicate = valid;
    exactDuplicate.koreanTeacherKeys =
        sourceKeys({arbitraryBytes, arbitraryBytes});
    if (!hasIssue(
            exactDuplicate,
            TeacherImportPlanValidationIssue::DuplicateKoreanTeacherName))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput canonicallyDistinct = valid;
    canonicallyDistinct.koreanTeacherKeys = {
        ImportSourceKey(std::string("\xC3\xA9")),
        ImportSourceKey(std::string("e\xCC\x81"))
    };
    if (!hasIssue(canonicallyDistinct, TeacherImportPlanValidationIssue::None))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput byteMismatchPrecedesDate = valid;
    byteMismatchPrecedesDate.sourceDateValid = false;
    byteMismatchPrecedesDate.koreanTeacherKeys = sourceKeys({std::string("\xC3\xA9")});
    byteMismatchPrecedesDate.reviewedKoreanTeacherKeys = sourceKeys({
        std::string("e\xCC\x81")
    });
    if (!hasIssue(
            byteMismatchPrecedesDate,
            TeacherImportPlanValidationIssue::ReviewSelectionMismatch))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
