#pragma once

#include "next/application/schedule_import_apply_review_decisions.h"
#include "next/application/schedule_import_plan_validation.h"

#include <expected>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

struct ScheduleImportApplyTime final
{
    std::u16string day;
    std::u16string startTime;
    std::u16string endTime;
};

struct ScheduleImportApplyCandidate final
{
    std::u16string teacherKey;
    std::u16string teacherName;
    std::vector<std::u16string> rooms;
    std::vector<std::u16string> importedColors;
    std::u16string grade;
    std::u16string level;
    std::vector<ScheduleImportApplyTime> times;
    std::vector<std::u16string> sourceCells;
    std::u16string meetingPatternError;
};

struct ScheduleImportApplySlotState final
{
    std::u16string day;
    std::u16string startTime;
    std::u16string state;
};

struct ScheduleImportApplyDiagnostic final
{
    std::u16string sheetName;
    std::u16string userName;
    std::u16string cellReference;
    std::u16string value;
    std::u16string message;
};

struct ScheduleImportApplyTeacher final
{
    std::u16string teacherKey;
    ScheduleImportReviewTeacherAction action = ScheduleImportReviewTeacherAction::Unselected;
    std::optional<Domain::TeacherId> targetTeacherId;
    std::u16string selectedRoom;
};

struct ScheduleImportApplyClass final
{
    int candidateIndex = -1;
    ScheduleImportReviewClassAction action = ScheduleImportReviewClassAction::Unselected;
    std::optional<Domain::ClassId> targetClassId;
    std::u16string classColor;
    std::u16string fontColor;
};

struct ScheduleImportApplyRequest final
{
    bool intensiveSchedule = false;
    ScheduleImportPlanIntensiveMode intensiveMode = ScheduleImportPlanIntensiveMode::UpdateExisting;
    std::u16string selectedUserName;
    bool saveProfileNameIfBlank = false;
    bool updateProfileName = false;
    bool diagnosticsAcknowledged = false;
    std::vector<ScheduleImportApplyCandidate> candidates;
    std::vector<ScheduleImportApplySlotState> intensiveSlotStates;
    std::vector<ScheduleImportApplyDiagnostic> diagnostics;
    std::vector<ScheduleImportApplyTeacher> teachers;
    std::vector<ScheduleImportApplyClass> classes;
};

struct ScheduleImportApplySummary final
{
    int teachersCreated = 0;
    int teachersUpdated = 0;
    int classesCreated = 0;
    int classesUpdated = 0;
    int classesSkipped = 0;
    int schedulesCleared = 0;
    int ignoredCells = 0;
    bool profileNameUpdated = false;
};

enum class ScheduleImportApplyTeacherTargetIssueCode
{
    ExistingTeacherMissingTarget,
    NonExistingTeacherHasTarget
};

struct ScheduleImportApplyTeacherTargetIssue final
{
    ScheduleImportApplyTeacherTargetIssueCode code;
    std::u16string teacherKey;
    std::optional<Domain::TeacherId> targetTeacherId;
};

struct ScheduleImportApplyFailure final
{
    std::u16string message;
    std::optional<ScheduleImportPlanEligibilityIssue> policyIssue;
    std::optional<ScheduleImportApplyTeacherTargetIssue> teacherTargetIssue;
};

using ScheduleImportApplyResult =
    std::expected<ScheduleImportApplySummary, ScheduleImportApplyFailure>;

class ScheduleImportApplyWritePort
{
public:
    virtual ~ScheduleImportApplyWritePort() = default;
    [[nodiscard]] virtual ScheduleImportApplyResult applyScheduleImport(
        const ScheduleImportApplyRequest& request
        ) const = 0;
};

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
