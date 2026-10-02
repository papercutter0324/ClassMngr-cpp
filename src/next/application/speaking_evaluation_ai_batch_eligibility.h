#pragma once

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
    int grade = 0;
    bool hasDidWellItem = false;
    bool hasNeedsImprovementItem = false;
};

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
    if (input.grade < 4 || input.grade > 6)
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
