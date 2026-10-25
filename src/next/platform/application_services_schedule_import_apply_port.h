#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/schedule_import_repository.h"
#include "next/application/schedule_import_apply_use_case.h"

#include <QString>

#include <utility>

namespace ClassMngr::Next::Platform
{

[[nodiscard]] inline Application::ScheduleImportApplyResult
scheduleImportApplyResult(
    const Result<ScheduleImportSummary>& result
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

class ApplicationServicesScheduleImportApplyPort final
    : public Application::ScheduleImportApplyWritePort
{
public:
    explicit ApplicationServicesScheduleImportApplyPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::ScheduleImportApplyResult applyScheduleImport(
        const Application::ScheduleImportApplyRequest& request
        ) const override
    {
        WorkspaceStore* const store =
            m_services ? m_services->workspaceStore() : nullptr;
        if (!store || !store->isOpen())
        {
            return unavailableFailure();
        }

        ScheduleImportRepository* const repository =
            store->scheduleImportRepository();
        if (!repository)
        {
            return unavailableFailure();
        }

        return scheduleImportTypedApplyResult(repository->applyTyped(request));
    }

private:
    [[nodiscard]] static Application::ScheduleImportApplyResult
    unavailableFailure()
    {
        return std::unexpected(Application::ScheduleImportApplyFailure{
            u"No Teacher Profile is open.",
            std::nullopt,
            std::nullopt,
            std::nullopt,
            Domain::ErrorCode::Technical
        });
    }

    ApplicationServices* m_services;
};

} // namespace ClassMngr::Next::Platform
