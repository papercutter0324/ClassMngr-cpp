#pragma once

#include "data/repositories/schedule_import_repository.h"
#include "next/application/schedule_import_apply_use_case.h"

#include <QString>

#include <expected>
#include <utility>

namespace ClassMngr::Next::Platform
{

[[nodiscard]] inline Application::ScheduleImportApplyResult
scheduleImportApplyResult(
    const std::expected<ScheduleImportSummary, QString>& result
    )
{
    if (!result)
    {
        Application::ScheduleImportApplyFailure failure;
        failure.message = result.error().toStdU16String();
        failure.code = Domain::ErrorCode::Technical;
        return std::unexpected(std::move(failure));
    }

    return Application::ScheduleImportApplySummary{
        Application::ScheduleImportAppliedCount{result->teachersCreated},
        Application::ScheduleImportAppliedCount{result->teachersUpdated},
        Application::ScheduleImportAppliedCount{result->classesCreated},
        Application::ScheduleImportAppliedCount{result->classesUpdated},
        Application::ScheduleImportAppliedCount{result->classesSkipped},
        Application::ScheduleImportAppliedCount{result->schedulesCleared},
        Application::ScheduleImportAppliedCount{result->ignoredCells},
        result->profileNameUpdated
    };
}

[[nodiscard]] inline Domain::ErrorCode scheduleImportApplyErrorCode(
    const RepositoryErrorCode code
    ) noexcept
{
    switch (code)
    {
    case RepositoryErrorCode::InvalidInput:
        return Domain::ErrorCode::InvalidInput;
    case RepositoryErrorCode::NotFound:
        return Domain::ErrorCode::NotFound;
    case RepositoryErrorCode::Conflict:
        return Domain::ErrorCode::Conflict;
    case RepositoryErrorCode::Validation:
        return Domain::ErrorCode::Validation;
    case RepositoryErrorCode::Technical:
        return Domain::ErrorCode::Technical;
    }
    return Domain::ErrorCode::Technical;
}

[[nodiscard]] inline Application::ScheduleImportApplyResult
scheduleImportTypedApplyResult(
    const ScheduleImportTypedApplyResult& result
    )
{
    if (!result)
    {
        const ScheduleImportTypedApplyFailure& failure = result.error();
        Application::ScheduleImportApplyFailure mapped;
        mapped.message = failure.repositoryError.diagnostic.toStdU16String();
        mapped.policyIssue = failure.policyIssue;
        mapped.teacherTargetIssue = failure.teacherTargetIssue;
        mapped.stateValidationError = failure.stateValidationError;
        mapped.code = scheduleImportApplyErrorCode(
            failure.repositoryError.code
            );
        return std::unexpected(std::move(mapped));
    }

    return Application::ScheduleImportApplySummary{
        Application::ScheduleImportAppliedCount{result->teachersCreated},
        Application::ScheduleImportAppliedCount{result->teachersUpdated},
        Application::ScheduleImportAppliedCount{result->classesCreated},
        Application::ScheduleImportAppliedCount{result->classesUpdated},
        Application::ScheduleImportAppliedCount{result->classesSkipped},
        Application::ScheduleImportAppliedCount{result->schedulesCleared},
        Application::ScheduleImportAppliedCount{result->ignoredCells},
        result->profileNameUpdated
    };
}

} // namespace ClassMngr::Next::Platform
