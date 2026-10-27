#pragma once

#include "domain/models/schedule_import.h"
#include "next/application/schedule_import_matching_projection.h"
#include "next/application/schedule_import_apply_request.h"
#include "next/application/schedule_import_plan_validation.h"
#include "next/application/schedule_import_review_readiness.h"
#include "next/application/schedule_import_state_snapshot.h"

#include <QString>

#include <expected>
#include <vector>

namespace ClassMngr::Next::Ui
{

struct ScheduleImportReviewMapping final
{
    ScheduleImportKind kind = ScheduleImportKind::Normal;
    ScheduleImportUserBlock user;
    Application::ScheduleImportStateSnapshot snapshot;
    Application::ScheduleImportMatchingProjection matching;
    Application::ScheduleImportApplyRequest applyTemplate;
    Application::ScheduleImportReviewDecisionRequest decisions;
    std::vector<std::optional<Domain::TeacherId>> selectedTeacherIds;
    std::vector<Application::ScheduleImportPlanEligibilityClassColors>
        classColors;
};

[[nodiscard]] std::expected<ScheduleImportReviewMapping, QString>
mapScheduleImportReview(
    const ScheduleImportUserBlock& user,
    ScheduleImportKind kind,
    const Application::ScheduleImportStateSnapshot& snapshot
    );

[[nodiscard]] Application::ScheduleImportApplyRequest
makeScheduleImportApplyRequest(
    const ScheduleImportReviewMapping& mapping,
    const Application::ScheduleImportReviewDecisionRequest& decisions,
    const std::vector<std::optional<Domain::TeacherId>>& teacherIds,
    const std::vector<Application::ScheduleImportPlanEligibilityClassColors>&
        classColors,
    Application::ScheduleImportPlanIntensiveMode intensiveMode,
    bool diagnosticsAcknowledged,
    bool saveProfileNameIfBlank,
    bool updateProfileName
    );

[[nodiscard]] Application::ScheduleImportStateValidationRequest
makeScheduleImportStateValidationRequest(
    const ScheduleImportReviewMapping& mapping,
    const Application::ScheduleImportReviewDecisionRequest& decisions,
    const std::vector<std::optional<Domain::TeacherId>>& teacherIds,
    Application::ScheduleImportPlanIntensiveMode intensiveMode
    );

[[nodiscard]] Application::ScheduleImportReviewReadinessResult
evaluateScheduleImportReview(
    const ScheduleImportReviewMapping& mapping,
    const Application::ScheduleImportReviewDecisionRequest& decisions,
    const std::vector<std::optional<Domain::TeacherId>>& teacherIds,
    Application::ScheduleImportPlanIntensiveMode intensiveMode
    );

} // namespace ClassMngr::Next::Ui
