#include "next/application/schedule_import_apply_use_case.h"

#include <stdexcept>

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
}
