#pragma once

#include "data/repositories/repository_error.h"
#include "domain/models/schedule_import.h"
#include "next/application/schedule_import_apply_request.h"
#include "next/application/schedule_import_state_validation.h"

#include <QSqlDatabase>

#include <expected>
#include <optional>
#include <utility>

struct ScheduleImportTypedApplyFailure final
{
    RepositoryError repositoryError;
    std::optional<
        ClassMngr::Next::Application::ScheduleImportPlanEligibilityIssue
        > policyIssue;
    std::optional<
        ClassMngr::Next::Application::ScheduleImportApplyTeacherTargetIssue
        > teacherTargetIssue;
    std::optional<
        ClassMngr::Next::Application::ScheduleImportStateValidationError
        > stateValidationError;
};

using ScheduleImportTypedApplyResult = std::expected<
    ScheduleImportSummary,
    ScheduleImportTypedApplyFailure
    >;

class ScheduleImportRepository
{
public:
    explicit ScheduleImportRepository(
        QSqlDatabase& database
        );

    [[nodiscard]] RepositoryResult<ScheduleImportPreview> preview(
        const ScheduleImportUserBlock& user,
        ScheduleImportKind kind
        );

    [[nodiscard]] RepositoryResult<ScheduleImportSummary> apply(
        const ScheduleImportPlan& plan
        );

    [[nodiscard]] ScheduleImportTypedApplyResult applyTyped(
        const ClassMngr::Next::Application::ScheduleImportApplyRequest& request
        );

private:
    using ApplyCoreResult = std::expected<
        ScheduleImportSummary,
        ScheduleImportTypedApplyFailure
        >;

    [[nodiscard]] ApplyCoreResult applyCore(
        const ClassMngr::Next::Application::ScheduleImportApplyRequest& request
        );

    QSqlDatabase& m_database;
};
