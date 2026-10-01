#pragma once

#include "next/application/schedule_import_review_decisions.h"
#include "next/application/schedule_import_state_validation.h"

#include <optional>

namespace ClassMngr::Next::Application
{

enum class ScheduleImportStateEvaluationStatus
{
    NotProvided,
    SkippedBecauseDecisionsRejected,
    Passed,
    Rejected
};

struct ScheduleImportReviewReadinessResult final
{
    ScheduleImportReviewDecisionResult decisions;
    ScheduleImportStateEvaluationStatus stateStatus =
        ScheduleImportStateEvaluationStatus::NotProvided;
    std::optional<ScheduleImportStateValidationError> stateError;
};

[[nodiscard]] inline ScheduleImportReviewReadinessResult
evaluateScheduleImportReviewReadiness(
    const ScheduleImportReviewDecisionRequest& decisions,
    const std::optional<ScheduleImportStateValidationRequest>& stateInput
    )
{
    ScheduleImportReviewReadinessResult result;
    result.decisions = validateScheduleImportReviewDecisions(decisions);
    if (!result.decisions.accepted())
    {
        result.stateStatus =
            ScheduleImportStateEvaluationStatus::
                SkippedBecauseDecisionsRejected;
        return result;
    }
    if (!stateInput)
    {
        result.stateStatus = ScheduleImportStateEvaluationStatus::NotProvided;
        return result;
    }

    result.stateError = validateScheduleImportState(*stateInput);
    result.stateStatus = result.stateError
        ? ScheduleImportStateEvaluationStatus::Rejected
        : ScheduleImportStateEvaluationStatus::Passed;
    return result;
}

} // namespace ClassMngr::Next::Application
