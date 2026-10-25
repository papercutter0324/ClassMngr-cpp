#pragma once

#include "next/application/schedule_import_applied_count.h"
#include "next/application/schedule_import_candidate_index.h"
#include "next/application/schedule_import_plan_validation.h"
#include "next/application/schedule_import_state_validation.h"
#include "next/domain/operation_result.h"

#include <expected>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

// Preserve the exact UTF-16 teacher-key code units supplied by the review
// adapter. The key is opaque here; workflow validation owns any interpretation.
class ScheduleImportApplyTeacherKey final
{
public:
    ScheduleImportApplyTeacherKey() = default;

    explicit ScheduleImportApplyTeacherKey(std::u16string value)
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_value.empty();
    }

    [[nodiscard]] const std::u16string& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const ScheduleImportApplyTeacherKey&,
        const ScheduleImportApplyTeacherKey&
        ) = default;

    friend bool operator<(
        const ScheduleImportApplyTeacherKey& left,
        const ScheduleImportApplyTeacherKey& right
        )
    {
        return left.m_value < right.m_value;
    }

private:
    std::u16string m_value;
};

struct ScheduleImportApplyTime final
{
    std::u16string day;
    std::u16string startTime;
    std::u16string endTime;
};

struct ScheduleImportApplyCandidate final
{
    ScheduleImportApplyTeacherKey teacherKey;
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
    ScheduleImportApplyTeacherKey teacherKey;
    ScheduleImportReviewTeacherAction action = ScheduleImportReviewTeacherAction::Unselected;
    std::optional<Domain::TeacherId> targetTeacherId;
    std::u16string selectedRoom;
};

struct ScheduleImportApplyClass final
{
    ScheduleImportCandidateIndex candidateIndex;
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
    ScheduleImportAppliedCount teachersCreated;
    ScheduleImportAppliedCount teachersUpdated;
    ScheduleImportAppliedCount classesCreated;
    ScheduleImportAppliedCount classesUpdated;
    ScheduleImportAppliedCount classesSkipped;
    ScheduleImportAppliedCount schedulesCleared;
    ScheduleImportAppliedCount ignoredCells;
    bool profileNameUpdated = false;
};

enum class ScheduleImportApplyTeacherTargetIssueCode
{
    ExistingTeacherMissingTarget,
    InvalidExistingTeacherTarget,
    NonExistingTeacherHasTarget
};

struct ScheduleImportApplyTeacherTargetIssue final
{
    ScheduleImportApplyTeacherTargetIssueCode code;
    ScheduleImportApplyTeacherKey teacherKey;
    std::optional<Domain::TeacherId> targetTeacherId;
};

struct ScheduleImportApplyFailure final
{
    std::u16string message;
    std::optional<ScheduleImportPlanEligibilityIssue> policyIssue;
    std::optional<ScheduleImportApplyTeacherTargetIssue> teacherTargetIssue;
    std::optional<ScheduleImportStateValidationError> stateValidationError;
    Domain::ErrorCode code = Domain::ErrorCode::Technical;
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

} // namespace ClassMngr::Next::Application
