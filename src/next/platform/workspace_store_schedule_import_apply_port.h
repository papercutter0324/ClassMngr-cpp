#pragma once

#include "data/database/workspace_store.h"
#include "next/application/schedule_import_apply_use_case.h"
#include "next/platform/schedule_import_apply_port_mapping.h"

namespace ClassMngr::Next::Platform
{

class WorkspaceStoreScheduleImportApplyPort final
    : public Application::ScheduleImportApplyWritePort
{
public:
    explicit WorkspaceStoreScheduleImportApplyPort(
        WorkspaceStore& store
        ) noexcept
        : m_store(&store)
    {
    }

    [[nodiscard]] Application::ScheduleImportApplyResult applyScheduleImport(
        const Application::ScheduleImportApplyRequest& request
        ) const override
    {
        if (!m_store || !m_store->isOpen())
        {
            return unavailableFailure();
        }

        ScheduleImportRepository* const repository =
            m_store->scheduleImportRepository();
        if (!repository)
        {
            return unavailableFailure();
        }

        // The repository owns the write transaction and repeats plan/state
        // validation against the current database snapshot before mutation.
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

    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform
