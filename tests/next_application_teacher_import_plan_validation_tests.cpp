#include "next/application/teacher_import_plan_validation.h"

#include <cstdlib>
#include <string>
#include <vector>

using ClassMngr::Next::Application::TeacherImportPlanValidationInput;
using ClassMngr::Next::Application::TeacherImportPlanValidationIssue;
using ClassMngr::Next::Application::validateTeacherImportPlan;

namespace
{

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
    valid.koreanTeacherKeys = {"korean-1", "korean-2"};
    valid.nativeEnglishTeacherKeys = {"native-1", "native-2"};
    valid.gsTeamMemberKeys = {
        {"english-1", "korean-1"},
        {"english-2", "korean-2"}
    };
    if (!hasIssue(valid, TeacherImportPlanValidationIssue::None))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput invalidDate = valid;
    invalidDate.sourceDateValid = false;
    invalidDate.koreanTeacherKeys = {""};
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
    reviewCountMismatch.reviewedKoreanTeacherKeys = {"korean-1"};
    if (!hasIssue(
            reviewCountMismatch,
            TeacherImportPlanValidationIssue::ReviewSelectionMismatch))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput reviewOrderMismatch = valid;
    reviewOrderMismatch.reviewedKoreanTeacherKeys = {"korean-2", "korean-1"};
    if (!hasIssue(
            reviewOrderMismatch,
            TeacherImportPlanValidationIssue::ReviewSelectionMismatch))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput koreanMissing = valid;
    koreanMissing.koreanTeacherKeys = {""};
    koreanMissing.nativeEnglishTeacherKeys = {""};
    if (!hasIssue(
            koreanMissing,
            TeacherImportPlanValidationIssue::MissingKoreanTeacherName))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput koreanDuplicate = valid;
    koreanDuplicate.koreanTeacherKeys = {"same", "same"};
    koreanDuplicate.nativeEnglishTeacherKeys = {""};
    if (!hasIssue(
            koreanDuplicate,
            TeacherImportPlanValidationIssue::DuplicateKoreanTeacherName))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput nativeMissing = valid;
    nativeMissing.nativeEnglishTeacherKeys = {""};
    nativeMissing.gsTeamMemberKeys = {{"", ""}};
    if (!hasIssue(
            nativeMissing,
            TeacherImportPlanValidationIssue::MissingNativeEnglishTeacherName))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput nativeDuplicate = valid;
    nativeDuplicate.nativeEnglishTeacherKeys = {"same", "same"};
    if (!hasIssue(
            nativeDuplicate,
            TeacherImportPlanValidationIssue::DuplicateNativeEnglishTeacherName))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput gsMissing = valid;
    gsMissing.gsTeamMemberKeys = {{"", ""}};
    if (!hasIssue(
            gsMissing,
            TeacherImportPlanValidationIssue::MissingGsTeamMemberName))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput gsEnglishDuplicate = valid;
    gsEnglishDuplicate.gsTeamMemberKeys = {
        {"same", "korean-1"},
        {"same", "korean-2"}
    };
    if (!hasIssue(
            gsEnglishDuplicate,
            TeacherImportPlanValidationIssue::DuplicateGsTeamMemberName))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput gsKoreanDuplicate = valid;
    gsKoreanDuplicate.gsTeamMemberKeys = {
        {"english-1", "same"},
        {"english-2", "same"}
    };
    if (!hasIssue(
            gsKoreanDuplicate,
            TeacherImportPlanValidationIssue::DuplicateGsTeamMemberName))
    {
        return EXIT_FAILURE;
    }

    TeacherImportPlanValidationInput gsNamespaces = valid;
    gsNamespaces.gsTeamMemberKeys = {
        {"shared", "other-1"},
        {"other-2", "shared"}
    };
    if (!hasIssue(gsNamespaces, TeacherImportPlanValidationIssue::None))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
