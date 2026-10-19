#pragma once

#include "next/domain/course.h"

namespace ClassMngr::Next::Application
{

enum class SpeakingEvaluationAiBatchEligibilityReason
{
    Eligible,
    MissingName,
    UnsupportedGrade,
    MissingDidWellItem,
    MissingNeedsImprovementItem
};

struct SpeakingEvaluationAiBatchEligibilityInput final
{
    bool hasTrimmedStudentName = false;
    Domain::CourseGradeBand grade = Domain::CourseGradeBand::Other;
    bool hasDidWellItem = false;
    bool hasNeedsImprovementItem = false;
};

[[nodiscard]] inline Domain::CourseGradeBand
speakingEvaluationCourseGradeBandFromInteger(const int grade) noexcept
{
    switch (grade)
    {
    case 4:
        return Domain::CourseGradeBand::E4;
    case 5:
        return Domain::CourseGradeBand::E5;
    case 6:
        return Domain::CourseGradeBand::E6;
    default:
        return Domain::CourseGradeBand::Other;
    }
}

// Returns the first unmet requirement in the order shown by the dialog.
[[nodiscard]] inline SpeakingEvaluationAiBatchEligibilityReason
speakingEvaluationAiBatchEligibilityReason(
    const SpeakingEvaluationAiBatchEligibilityInput& input
    ) noexcept
{
    if (!input.hasTrimmedStudentName)
    {
        return SpeakingEvaluationAiBatchEligibilityReason::MissingName;
    }
    if (input.grade != Domain::CourseGradeBand::E4
        && input.grade != Domain::CourseGradeBand::E5
        && input.grade != Domain::CourseGradeBand::E6)
    {
        return SpeakingEvaluationAiBatchEligibilityReason::UnsupportedGrade;
    }
    if (!input.hasDidWellItem)
    {
        return SpeakingEvaluationAiBatchEligibilityReason::MissingDidWellItem;
    }
    if (!input.hasNeedsImprovementItem)
    {
        return SpeakingEvaluationAiBatchEligibilityReason::
            MissingNeedsImprovementItem;
    }
    return SpeakingEvaluationAiBatchEligibilityReason::Eligible;
}

} // namespace ClassMngr::Next::Application
