#include "next/application/schedule_import_review_readiness.h"

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

using namespace ClassMngr::Next::Application;

namespace
{
using DecisionIssueCode = ScheduleImportReviewDecisionIssueCode;
using StateAction = ScheduleImportStateTeacherAction;
using StateErrorCode = ScheduleImportStateValidationErrorCode;
using StateStatus = ScheduleImportStateEvaluationStatus;

void require(bool condition, const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

ScheduleImportReviewDecisionRequest invalidDecisions()
{
    ScheduleImportReviewDecisionRequest request;
    request.candidates = {{"teacher-a", {}}};
    return request;
}

ScheduleImportStateValidationRequest invalidStateInput()
{
    ScheduleImportStateValidationRequest request;
    request.teacherResolutions.push_back(
        {"teacher-a", StateAction::Reuse, std::nullopt, false}
        );
    return request;
}
}

int main()
{
    int cases = 0;
    try
    {
        {
            const auto result = evaluateScheduleImportReviewReadiness(
                invalidDecisions(),
                invalidStateInput()
                );
            require(!result.decisions.accepted(),
                    "invalid decisions were accepted");
            require(
                result.decisions.issues.front().code
                    == DecisionIssueCode::MissingTeacherResolution,
                "decision rejection returned the wrong issue"
                );
            require(
                result.stateStatus
                    == StateStatus::SkippedBecauseDecisionsRejected,
                "state validation did not defer to decision rejection"
                );
            require(!result.stateError,
                    "state validation ran after rejected decisions");
            ++cases;
        }
        {
            ScheduleImportStateValidationRequest invalidState =
                invalidStateInput();
            ScheduleImportReviewDecisionRequest validDecisions;
            const auto result = evaluateScheduleImportReviewReadiness(
                validDecisions,
                invalidState
                );
            require(result.decisions.accepted(),
                    "valid decisions were rejected");
            require(result.stateStatus == StateStatus::Rejected,
                    "invalid state input was not rejected");
            require(
                result.stateError
                    && result.stateError->code
                        == StateErrorCode::SelectedTeacherUnavailable,
                "state rejection returned the wrong error"
                );
            ++cases;
        }
        {
            ScheduleImportReviewDecisionRequest validDecisions;
            const auto result = evaluateScheduleImportReviewReadiness(
                validDecisions,
                ScheduleImportStateValidationRequest{}
                );
            require(result.decisions.accepted(),
                    "empty valid decision set was rejected");
            require(result.stateStatus == StateStatus::Passed,
                    "valid state input did not pass");
            require(!result.stateError,
                    "valid state input returned an error");
            ++cases;
        }
        {
            ScheduleImportReviewDecisionRequest validDecisions;
            const auto result = evaluateScheduleImportReviewReadiness(
                validDecisions,
                std::nullopt
                );
            require(result.decisions.accepted(),
                    "valid decisions were rejected without state input");
            require(result.stateStatus == StateStatus::NotProvided,
                    "missing state input had the wrong evaluation status");
            require(!result.stateError,
                    "missing state input returned an error");
            ++cases;
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }

    std::cout << cases << " Schedule Import readiness cases passed\n";
    return 0;
}
