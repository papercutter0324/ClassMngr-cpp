#pragma once

#include "next/application/schedule_import_apply_review_decisions.h"
#include "next/application/schedule_import_apply_request.h"
#include "next/application/schedule_import_plan_validation.h"

#include <utility>

namespace ClassMngr::Next::Application
{

class ScheduleImportApplyUseCase final
{
public:
    [[nodiscard]] static ScheduleImportApplyResult execute(
        const ScheduleImportApplyRequest& request,
        const ScheduleImportApplyWritePort& port
        )
    {
        ScheduleImportPlanEligibilityRequest policy;
        policy.intensiveSchedule = request.intensiveSchedule;
        policy.intensiveMode = request.intensiveMode;
        policy.hasDiagnostics = !request.diagnostics.empty();
        policy.diagnosticsAcknowledged = request.diagnosticsAcknowledged;
        policy.reviewDecisions =
            projectScheduleImportApplyReviewDecisions(request);
        for (const auto& candidate : request.candidates)
        {
            ScheduleImportPlanEligibilityCandidate item;
            item.grade = scheduleImportApplyUtf8(candidate.grade);
            item.level = scheduleImportApplyUtf8(candidate.level);
            item.teacherKey = candidate.teacherKey;
            item.teacherName = candidate.teacherName;
            for (const auto& time : candidate.times)
            {
                item.weekdays.push_back(scheduleImportApplyUtf8(time.day));
            }
            policy.candidates.push_back(std::move(item));
        }
        for (const auto& candidateClass : request.classes)
        {
            policy.classColors.push_back({
                candidateClass.candidateIndex,
                scheduleImportApplyUtf8(candidateClass.classColor),
                scheduleImportApplyUtf8(candidateClass.fontColor)
            });
        }
        if (auto issue = validateScheduleImportPlanEligibility(policy))
        {
            return std::unexpected(ScheduleImportApplyFailure{
                u"The import plan contains an invalid resolution.", std::move(issue)
            });
        }
        for (const auto& teacher : request.teachers)
        {
            const bool needsTarget =
                teacher.action == ScheduleImportReviewTeacherAction::Reuse
                || teacher.action == ScheduleImportReviewTeacherAction::UpdateRoom;
            if (needsTarget && !teacher.targetTeacherId)
            {
                return std::unexpected(ScheduleImportApplyFailure{
                    u"Choose an existing Korean teacher for this resolution.",
                    std::nullopt,
                    ScheduleImportApplyTeacherTargetIssue{
                        ScheduleImportApplyTeacherTargetIssueCode::ExistingTeacherMissingTarget,
                        teacher.teacherKey, teacher.targetTeacherId}
                });
            }
            if (!needsTarget && teacher.targetTeacherId)
            {
                return std::unexpected(ScheduleImportApplyFailure{
                    u"This Korean teacher resolution cannot use an existing teacher.",
                    std::nullopt,
                    ScheduleImportApplyTeacherTargetIssue{
                        ScheduleImportApplyTeacherTargetIssueCode::NonExistingTeacherHasTarget,
                        teacher.teacherKey, teacher.targetTeacherId}
                });
            }
        }
        return port.applyScheduleImport(request);
    }
};

} // namespace ClassMngr::Next::Application
