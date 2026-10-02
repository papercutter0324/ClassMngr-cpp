#pragma once

#include "next/application/schedule_import_plan_validation.h"
#include "next/application/schedule_import_state_validation.h"

#include <expected>
#include <optional>
#include <string>
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
    std::optional<ScheduleImportStateValidationError> stateValidationError;
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
