#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/schedule_import_repository.h"
#include "next/application/schedule_import_apply_use_case.h"

#include <QString>

namespace ClassMngr::Next::Platform
{

[[nodiscard]] inline Application::ScheduleImportApplyResult
scheduleImportApplyResult(
    const Result<ScheduleImportSummary>& result
    )
{
    if (!result)
    {
        return std::unexpected(Application::ScheduleImportApplyFailure{
            result.error().toStdU16String(),
            std::nullopt
        });
    }

    return Application::ScheduleImportApplySummary{
        result->teachersCreated,
        result->teachersUpdated,
        result->classesCreated,
        result->classesUpdated,
        result->classesSkipped,
        result->schedulesCleared,
        result->ignoredCells,
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
        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return unavailableFailure();
        }

        ScheduleImportRepository* const repository =
            session->scheduleImportRepository();
        if (!repository)
        {
            return unavailableFailure();
        }

        return scheduleImportApplyResult(repository->applyTyped(request));
    }

private:
    [[nodiscard]] static Application::ScheduleImportApplyResult
    unavailableFailure()
    {
        return std::unexpected(Application::ScheduleImportApplyFailure{
            u"No Teacher Profile is open.",
            std::nullopt
        });
    }

    ApplicationServices* m_services;
};

} // namespace ClassMngr::Next::Platform
