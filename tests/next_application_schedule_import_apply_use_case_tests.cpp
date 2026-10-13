#include "next/application/schedule_import_apply_use_case.h"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

using namespace ClassMngr::Next::Application;

namespace
{
void require(bool condition)
{
    if (!condition) throw std::runtime_error("Schedule import apply assertion failed");
}

struct Port final : ScheduleImportApplyWritePort
{
    mutable int calls = 0;
    ScheduleImportApplyResult result = ScheduleImportApplySummary{1, 2, 3, 4, 5, 6, 7, true};

    ScheduleImportApplyResult applyScheduleImport(
        const ScheduleImportApplyRequest&) const override
    {
        ++calls;
        return result;
    }
};

ScheduleImportApplyRequest validRequest()
{
    ScheduleImportApplyRequest request;
    request.candidates.push_back({
        u"\uAE40\uC120\uC0DD", u"\uAE40\uC120\uC0DD", {u"413"}, {}, u"E4", u"Hercules",
        {{u"Monday", u"4:00 PM", u"4:50 PM"},
         {u"Wednesday", u"4:00 PM", u"4:50 PM"}}, {}, {}});
    request.teachers.push_back({u"\uAE40\uC120\uC0DD", ScheduleImportReviewTeacherAction::Create,
        std::nullopt, u"413"});
    request.classes.push_back({0, ScheduleImportReviewClassAction::CreateNew,
        std::nullopt, u"#FFFFFF", u"#000000"});
    return request;
}
}

int main()
{
    Port port;
    const auto success = ScheduleImportApplyUseCase::execute(validRequest(), port);
    require(success && success->teachersCreated == 1 && success->classesUpdated == 4
        && success->ignoredCells == 7 && success->profileNameUpdated);
    require(port.calls == 1);

    auto invalidNormalScheduleMode = validRequest();
    invalidNormalScheduleMode.intensiveMode =
        ScheduleImportPlanIntensiveMode::Invalid;
    const auto rejectedNormalScheduleMode =
        ScheduleImportApplyUseCase::execute(invalidNormalScheduleMode, port);
    require(!rejectedNormalScheduleMode
        && rejectedNormalScheduleMode.error().policyIssue.has_value());
    require(rejectedNormalScheduleMode.error().policyIssue->code
        == ScheduleImportPlanEligibilityIssueCode::InvalidIntensiveMode);
    require(port.calls == 1);

    const auto teacherId = ClassMngr::Next::Domain::TeacherId::fromString("7");
    require(teacherId.has_value());
    Port targetPort;
    int expectedTargetCalls = 0;
    for (const auto action : {
             ScheduleImportReviewTeacherAction::Reuse,
             ScheduleImportReviewTeacherAction::UpdateRoom})
    {
        auto missingTeacherTarget = validRequest();
        missingTeacherTarget.teachers[0].action = action;
        const auto rejectedTarget = ScheduleImportApplyUseCase::execute(
            missingTeacherTarget, targetPort);
        require(!rejectedTarget && rejectedTarget.error().teacherTargetIssue.has_value());
        require(rejectedTarget.error().teacherTargetIssue->code
            == ScheduleImportApplyTeacherTargetIssueCode::ExistingTeacherMissingTarget);
        require(!rejectedTarget.error().policyIssue.has_value());
        require(rejectedTarget.error().message
            == u"Choose an existing Korean teacher for this resolution.");
        require(targetPort.calls == expectedTargetCalls);

        missingTeacherTarget.teachers[0].targetTeacherId = teacherId;
        const auto acceptedTarget = ScheduleImportApplyUseCase::execute(
            missingTeacherTarget, targetPort);
        ++expectedTargetCalls;
        require(acceptedTarget && targetPort.calls == expectedTargetCalls);
    }
    for (const auto action : {
             ScheduleImportReviewTeacherAction::Reuse,
             ScheduleImportReviewTeacherAction::UpdateRoom})
    {
        for (const auto& invalidId : {
                 std::string("not-a-number"),
                 std::string("9223372036854775808")})
        {
            auto invalidTeacherId = validRequest();
            invalidTeacherId.teachers[0].action = action;
            invalidTeacherId.teachers[0].targetTeacherId =
                ClassMngr::Next::Domain::TeacherId::fromString(invalidId);
            const auto rejectedId = ScheduleImportApplyUseCase::execute(
                invalidTeacherId,
                targetPort
                );
            require(!rejectedId
                && rejectedId.error().teacherTargetIssue.has_value());
            require(rejectedId.error().teacherTargetIssue->code
                == ScheduleImportApplyTeacherTargetIssueCode::InvalidExistingTeacherTarget);
            require(rejectedId.error().teacherTargetIssue->targetTeacherId
                == invalidTeacherId.teachers[0].targetTeacherId);
            require(!rejectedId.error().policyIssue.has_value());
            require(rejectedId.error().message
                == u"Choose a valid existing Korean teacher for this resolution.");
            require(targetPort.calls == expectedTargetCalls);
        }
    }
    const auto maxTeacherId = ClassMngr::Next::Domain::TeacherId::fromString(
        std::to_string(std::numeric_limits<std::int64_t>::max())
        );
    require(maxTeacherId.has_value());
    for (const auto action : {
             ScheduleImportReviewTeacherAction::Reuse,
             ScheduleImportReviewTeacherAction::UpdateRoom})
    {
        auto boundaryTeacherId = validRequest();
        boundaryTeacherId.teachers[0].action = action;
        boundaryTeacherId.teachers[0].targetTeacherId = maxTeacherId;
        const auto acceptedBoundary = ScheduleImportApplyUseCase::execute(
            boundaryTeacherId,
            targetPort
            );
        ++expectedTargetCalls;
        require(acceptedBoundary && targetPort.calls == expectedTargetCalls);
    }
    for (const auto& invalidId : {
             std::string("not-a-number"),
             std::string("2147483648")})
    {
        auto invalidClassId = validRequest();
        invalidClassId.classes[0].action =
            ScheduleImportReviewClassAction::UpdateExisting;
        invalidClassId.classes[0].targetClassId =
            ClassMngr::Next::Domain::ClassId::fromString(invalidId);
        const auto rejectedId = ScheduleImportApplyUseCase::execute(
            invalidClassId,
            targetPort
            );
        require(!rejectedId && rejectedId.error().policyIssue.has_value());
        require(rejectedId.error().policyIssue->code
            == ScheduleImportPlanEligibilityIssueCode::InvalidReviewDecision);
        require(rejectedId.error().policyIssue->reviewDecisionIssue.has_value());
        require(rejectedId.error().policyIssue->reviewDecisionIssue->code
            == ScheduleImportReviewDecisionIssueCode::InvalidExistingClassTarget);
        require(rejectedId.error().policyIssue->reviewDecisionIssue->targetClassId
            == invalidClassId.classes[0].targetClassId);
        require(targetPort.calls == expectedTargetCalls);
    }
    auto invalidSkippedClassId = validRequest();
    invalidSkippedClassId.classes[0].action =
        ScheduleImportReviewClassAction::Skip;
    invalidSkippedClassId.classes[0].targetClassId =
        ClassMngr::Next::Domain::ClassId::fromString("not-a-number");
    const auto rejectedSkippedClassId = ScheduleImportApplyUseCase::execute(
        invalidSkippedClassId,
        targetPort
        );
    require(!rejectedSkippedClassId
        && rejectedSkippedClassId.error().policyIssue.has_value());
    require(rejectedSkippedClassId.error().policyIssue->reviewDecisionIssue
        ->code == ScheduleImportReviewDecisionIssueCode::InvalidExistingClassTarget);
    require(targetPort.calls == expectedTargetCalls);

    auto invalidCreateTarget = validRequest();
    invalidCreateTarget.classes[0].targetClassId =
        ClassMngr::Next::Domain::ClassId::fromString("not-a-number");
    const auto rejectedCreateTarget = ScheduleImportApplyUseCase::execute(
        invalidCreateTarget,
        targetPort
        );
    require(!rejectedCreateTarget
        && rejectedCreateTarget.error().policyIssue.has_value());
    require(rejectedCreateTarget.error().policyIssue->reviewDecisionIssue
        ->code == ScheduleImportReviewDecisionIssueCode::CreateNewClassHasTarget);
    require(targetPort.calls == expectedTargetCalls);

    auto boundaryClassId = validRequest();
    boundaryClassId.classes[0].action =
        ScheduleImportReviewClassAction::UpdateExisting;
    boundaryClassId.classes[0].targetClassId =
        ClassMngr::Next::Domain::ClassId::fromString(
            std::to_string(std::numeric_limits<int>::max())
            );
    const auto acceptedBoundaryClassId = ScheduleImportApplyUseCase::execute(
        boundaryClassId,
        targetPort
        );
    ++expectedTargetCalls;
    require(acceptedBoundaryClassId
        && targetPort.calls == expectedTargetCalls);
    for (const auto action : {
             ScheduleImportReviewTeacherAction::Create,
             ScheduleImportReviewTeacherAction::Skip})
    {
        auto unexpectedTeacherTarget = validRequest();
        unexpectedTeacherTarget.teachers[0].action = action;
        unexpectedTeacherTarget.teachers[0].targetTeacherId = teacherId;
        if (action == ScheduleImportReviewTeacherAction::Skip)
            unexpectedTeacherTarget.classes[0].action = ScheduleImportReviewClassAction::Skip;
        const auto rejectedTarget = ScheduleImportApplyUseCase::execute(
            unexpectedTeacherTarget, targetPort);
        require(!rejectedTarget && rejectedTarget.error().teacherTargetIssue.has_value());
        require(rejectedTarget.error().teacherTargetIssue->code
            == ScheduleImportApplyTeacherTargetIssueCode::NonExistingTeacherHasTarget);
        require(rejectedTarget.error().teacherTargetIssue->targetTeacherId == teacherId);
        require(!rejectedTarget.error().policyIssue.has_value());
        require(rejectedTarget.error().message
            == u"This Korean teacher resolution cannot use an existing teacher.");
        require(targetPort.calls == expectedTargetCalls);
    }

    auto invalid = validRequest();
    invalid.diagnostics.push_back({});
    const auto rejected = ScheduleImportApplyUseCase::execute(invalid, port);
    require(!rejected && rejected.error().policyIssue.has_value());
    require(rejected.error().policyIssue->code
        == ScheduleImportPlanEligibilityIssueCode::UnacknowledgedDiagnostics);
    require(port.calls == 1);

    invalid.intensiveMode = ScheduleImportPlanIntensiveMode::Invalid;
    const auto rejectedDiagnosticsBeforeNormalMode =
        ScheduleImportApplyUseCase::execute(invalid, port);
    require(!rejectedDiagnosticsBeforeNormalMode
        && rejectedDiagnosticsBeforeNormalMode.error().policyIssue->code
            == ScheduleImportPlanEligibilityIssueCode::UnacknowledgedDiagnostics);
    require(port.calls == 1);

    invalid = validRequest();
    invalid.classes[0].action = ScheduleImportReviewClassAction::UpdateExisting;
    const auto missingTarget = ScheduleImportApplyUseCase::execute(invalid, port);
    require(!missingTarget && missingTarget.error().policyIssue.has_value());
    require(missingTarget.error().policyIssue->code
        == ScheduleImportPlanEligibilityIssueCode::InvalidReviewDecision);
    require(port.calls == 1);

    port.result = std::unexpected(ScheduleImportApplyFailure{u"write failed", std::nullopt});
    const auto failed = ScheduleImportApplyUseCase::execute(validRequest(), port);
    require(!failed && failed.error().message == u"write failed" && port.calls == 2);

    ScheduleImportApplyFailure stateFailure;
    stateFailure.message = u"The proposed schedule overlaps.";
    stateFailure.stateValidationError = ScheduleImportStateValidationError{
        ScheduleImportStateValidationErrorCode::ProjectedScheduleOverlap,
        "E5 Zeus",
        "E5 Apollo",
        "Monday",
        "4:00 PM",
        "4:55 PM"
    };
    port.result = std::unexpected(stateFailure);
    const auto stateFailed = ScheduleImportApplyUseCase::execute(
        validRequest(),
        port
        );
    require(!stateFailed && port.calls == 3);
    require(stateFailed.error().stateValidationError.has_value());
    require(stateFailed.error().stateValidationError->code
        == ScheduleImportStateValidationErrorCode::ProjectedScheduleOverlap);
    require(stateFailed.error().stateValidationError->classLabel == "E5 Zeus");
    require(stateFailed.error().stateValidationError->conflictingClassLabel
        == "E5 Apollo");
    require(stateFailed.error().stateValidationError->day == "Monday");
    require(stateFailed.error().stateValidationError->startTime == "4:00 PM");
    require(stateFailed.error().stateValidationError->endTime == "4:55 PM");
}
