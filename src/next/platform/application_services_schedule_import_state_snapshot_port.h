#pragma once

#include "core/application_services.h"
#include "next/platform/workspace_store_schedule_import_state_snapshot_port.h"

namespace ClassMngr::Next::Platform
{

// Compatibility adapter for legacy callers. Next setup uses the direct
// WorkspaceStore adapter and does not depend on ApplicationServices.
class ApplicationServicesScheduleImportStateSnapshotPort final
    : public Application::ScheduleImportStateSnapshotReadPort
{
public:
    explicit ApplicationServicesScheduleImportStateSnapshotPort(
        ApplicationServices* services
        ) noexcept
        : m_storePort(services ? services->workspaceStore() : nullptr)
    {
    }

    explicit ApplicationServicesScheduleImportStateSnapshotPort(
        ApplicationServices& services
        ) noexcept
        : ApplicationServicesScheduleImportStateSnapshotPort(&services)
    {
    }

    [[nodiscard]] Application::ScheduleImportStateSnapshotOutcome
    readCurrentScheduleImportState(
        const Application::ScheduleImportStateSnapshotQuery& query
        ) const override
    {
        return m_storePort.readCurrentScheduleImportState(query);
    }

private:
    WorkspaceStoreScheduleImportStateSnapshotPort m_storePort;
};

} // namespace ClassMngr::Next::Platform
